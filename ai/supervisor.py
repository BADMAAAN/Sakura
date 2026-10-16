"""Native-Windows, single-task FLUX supervisor (stdlib only)."""
from __future__ import annotations

import argparse, json, os, re, shutil, subprocess, sys, tempfile, time, urllib.error, urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]; AI=ROOT/'ai'
STATE=AI/'state'/'state.json'; QUEUE=AI/'tasks'/'queue.json'; LOCK=ROOT/'.ai-work.lock'
CHECKPOINTS=AI/'checkpoints'; LOGS=AI/'logs'
OLLAMA_URL=os.environ.get('FLUX_OLLAMA_URL','http://127.0.0.1:11434')
EXECUTORS=['codex','aider']
MODELS=['qwen2.5-coder:14b','qwen2.5:latest']
REVIEWER_MODEL='qwen2.5:14b'
FAILURE_MARKERS=('usage limit','rate limit','quota exhausted','http 429','429 too many','authentication','temporary failure','abnormal','not recognized','not found')

def now(): return datetime.now(timezone.utc).isoformat()
def decode_text(data):
    if data.startswith(b'\xff\xfe'):
        text=data[2:].decode('utf-16-le',errors='replace')
    elif data.startswith(b'\xfe\xff'):
        text=data[2:].decode('utf-16-be',errors='replace')
    else:
        sample=data[:4096]
        even_nuls=sum(byte==0 for byte in sample[0::2]); odd_nuls=sum(byte==0 for byte in sample[1::2])
        pairs=max(1,len(sample)//2)
        if odd_nuls/pairs >= .3 and even_nuls/pairs <= .1:
            text=data.decode('utf-16-le',errors='replace')
        elif even_nuls/pairs >= .3 and odd_nuls/pairs <= .1:
            text=data.decode('utf-16-be',errors='replace')
        else:
            text=data.decode('utf-8',errors='replace')
    return text.replace('\x00','')
def read_text(path): return decode_text(path.read_bytes())
def load(path,default):
    try: return json.loads(read_text(path))
    except (OSError,json.JSONDecodeError,UnicodeError): return default
def save(path,value):
    path.parent.mkdir(parents=True,exist_ok=True); fd,name=tempfile.mkstemp(prefix='.tmp-',dir=path.parent)
    try:
        with os.fdopen(fd,'w',encoding='utf-8',newline='\n') as f: json.dump(value,f,indent=2,ensure_ascii=False); f.write('\n')
        os.replace(name,path)
    finally:
        if os.path.exists(name): os.unlink(name)
def current_task():
    q=load(QUEUE,{'tasks':[]}).get('tasks',[]); done={x.get('id') for x in q if x.get('status')=='done'}
    return next((x for x in q if x.get('status') in ('in_progress','pending') and all(d in done for d in x.get('dependencies',[]))),None)
def pid_alive(pid):
    try: os.kill(int(pid),0); return True
    except (OSError,ValueError,TypeError,PermissionError): return False
def acquire(executor,task):
    recovered=False
    if LOCK.exists():
        old=load(LOCK,{})
        if pid_alive(old.get('pid')): raise RuntimeError(f'writer lock held by pid {old.get("pid")}')
        LOCK.unlink(missing_ok=True); recovered=True
    with LOCK.open('x',encoding='utf-8') as f: json.dump({'executor':executor,'pid':os.getpid(),'timestamp':now(),'task_id':task['id']},f)
    return recovered
def release(): LOCK.unlink(missing_ok=True)
def failure_kind(text):
    low=text.casefold()
    return 'unavailable' if any(x in low for x in FAILURE_MARKERS) or 'try again at' in low or 'goal hit usage limits' in low else ('semantic' if not text.strip() or 'malformed tool' in low else 'executor')
def retry_after(text):
    m=re.search(r'try again at\s+([^\r\n]+)',text,re.I); return m.group(1).strip() if m else None
def process_text(value): return str(value).replace('\x00','')
def run(cmd,name,env=None,timeout=3600):
    started=time.monotonic(); output=''; code=127
    safe_cmd=[process_text(x) for x in cmd]
    safe_env={process_text(key):process_text(value) for key,value in env.items()} if env is not None else None
    try:
        p=subprocess.run(safe_cmd,cwd=ROOT,text=True,capture_output=True,timeout=timeout,env=safe_env); output=(p.stdout or '')+'\n'+(p.stderr or ''); code=p.returncode
    except Exception as exc: output=str(exc)
    result={'executor':name,'command':safe_cmd,'exit_code':code,'duration_seconds':round(time.monotonic()-started,2),'output':output[-20000:]}
    d=LOGS/name; d.mkdir(parents=True,exist_ok=True); save(d/f'{time.time_ns()}.json',result); return result
def checkpoint(state,task,summary,next_action,evidence=None):
    changed=subprocess.run(['git','status','--short'],cwd=ROOT,text=True,capture_output=True).stdout
    diff=subprocess.run(['git','diff','--stat'],cwd=ROOT,text=True,capture_output=True).stdout
    data={'timestamp':now(),'current_task':task['id'],'executor':state.get('executor'),'model':state.get('model'),'summary':summary,'files_changed':changed,'git_diff_summary':diff,'build_result':state.get('last_build'),'test_result':state.get('last_test'),'review_result':state.get('last_review'),'next_action':next_action,'evidence':evidence or {}}
    path=CHECKPOINTS/f'{time.time_ns()}.json'; save(path,data); state['last_checkpoint']=str(path.relative_to(ROOT)); return path
def update(state): state['last_progress_at']=now(); state['heartbeat']=now(); save(STATE,state)
def find_aider():
    candidates=[]
    if os.environ.get('FLUX_AIDER_EXE'): candidates.append(Path(os.environ['FLUX_AIDER_EXE']))
    found=shutil.which('aider')
    if found: candidates.append(Path(found))
    candidates.extend([Path.home()/'.local'/'bin'/'aider.exe',Path.home()/'.local'/'bin'/'aider'])
    return next((str(x.resolve()) for x in candidates if x.is_file()),None)
def executor_cmd(name,prompt,model):
    if name=='codex': return ['codex','--ask-for-approval','never','exec','-C',str(ROOT),'-s','workspace-write','--ephemeral','--json',prompt]
    aider=find_aider()
    if not aider: raise FileNotFoundError('Aider executable not found; run ai/.venv/Scripts/aider-install.exe')
    command=[aider,'--yes-always','--no-git','--no-auto-commits','--no-gitignore','--no-check-update','--no-show-model-warnings','--map-tokens','0','--model',f'ollama_chat/{model}']
    metadata=ROOT/'.aider.model.metadata.json'
    if metadata.is_file(): command.extend(['--model-metadata-file',str(metadata)])
    return command+['--message',prompt]
def continuation_context(task,state):
    status=subprocess.run(['git','status','--short'],cwd=ROOT,text=True,capture_output=True).stdout
    diff=subprocess.run(['git','diff','--stat'],cwd=ROOT,text=True,capture_output=True).stdout
    checkpoint_data=load(ROOT/state.get('last_checkpoint',''),{}) if state.get('last_checkpoint') else {}
    build_log=AI/'logs'/'build'/'latest.log'
    build_tail=read_text(build_log)[-12000:] if build_log.is_file() else ''
    return (
        f"CURRENT_TASK={task['id']}\n{task['description']}\n"
        "Continue the same task and checkpoint. Fix only actual compiler/linker errors; add no features. "
        "After each logical fix rebuild; after GREEN run tests and request reviewer approval. "
        "Do not push or flash hardware.\n"
        f"STATE={json.dumps(state,ensure_ascii=False)}\n"
        f"LAST_CHECKPOINT={json.dumps(checkpoint_data,ensure_ascii=False)}\n"
        f"GIT_STATUS=\n{status}\nGIT_DIFF_STAT=\n{diff}\nBUILD_LOG_TAIL=\n{build_tail}"
    )
def ollama_generate(model,prompt,timeout=600):
    body=json.dumps({'model':model,'prompt':prompt,'stream':False,'options':{'temperature':0}}).encode()
    req=urllib.request.Request(f'{OLLAMA_URL}/api/generate',data=body,headers={'Content-Type':'application/json'}); started=time.monotonic()
    try:
        with urllib.request.urlopen(req,timeout=timeout) as response: value=json.loads(response.read().decode('utf-8'))
        return {'model':model,'exit_code':0,'response':value.get('response',''),'duration_seconds':round(time.monotonic()-started,2)}
    except (OSError,urllib.error.URLError,json.JSONDecodeError) as exc:
        return {'model':model,'exit_code':1,'response':str(exc),'duration_seconds':round(time.monotonic()-started,2)}
def choose_model(state,models=None):
    result={}
    for model in models or MODELS:
        result=ollama_generate(model,'Reply with exactly FLUX_MODEL_READY')
        state.setdefault('model_attempts',{})[model]=state.get('model_attempts',{}).get(model,0)+1
        if result['exit_code']==0 and 'FLUX_MODEL_READY' in result['response']:
            state.update({'model':model,'ollama_status':'ready'}); update(state); return model,result
        state.setdefault('model_failures',{})[model]=state.get('model_failures',{}).get(model,0)+1; update(state)
    return None,result
def run_tests(state):
    result=run([sys.executable,'-m','unittest','discover','-s','tests','-v'],'tests')
    state['last_test']={'exit_code':result['exit_code'],'output':result['output'][-12000:]}; update(state); return result
def review(state,task):
    prompt=f"You are the FLUX reviewer. Reply PASS or FAIL on the first line, then a concise reason. Review safe executor failover evidence. CURRENT_TASK={task['id']} executor={state.get('executor')} tests={state.get('last_test')}"
    result=ollama_generate(REVIEWER_MODEL,prompt); state['reviewer_model']=REVIEWER_MODEL; state['last_review']=result; update(state); return result
def mark_done(task):
    q=load(QUEUE,{'tasks':[]})
    for item in q.get('tasks',[]):
        if item.get('id')==task['id']: item['status']='done'
    save(QUEUE,q)
def recover_lock_probe():
    state=load(STATE,{}); task=current_task()
    if not task: return 1
    recovered=acquire('probe',task)
    release()
    state['stale_lock_recovered_at']=now() if recovered else None
    state['recovery_task_preserved']=state.get('current_task')==task['id']=='ai-infrastructure-001'
    checkpoint(state,task,'stale lock recovery probe','continue validation',{'recovered':recovered}); update(state)
    return 0 if recovered and state['recovery_task_preserved'] else 1
def watchdog_probe(marker):
    marker=Path(marker)
    if not marker.exists():
        marker.parent.mkdir(parents=True,exist_ok=True); marker.write_text('first-process-crashed\n',encoding='utf-8'); return 23
    state=load(STATE,{})
    state['watchdog_restart_verified_at']=now()
    state['recovery_task_preserved']=state.get('current_task')=='ai-infrastructure-001'
    update(state); return 0 if state['recovery_task_preserved'] else 1
def run_cycle(simulate_codex_unavailable=False,prompt=None,complete=False,model_order=None,probe_file=None,probe_contents=None):
    state=load(STATE,{}); task=current_task()
    if not task: return 0
    if task.get('status')=='pending':
        task['status']='in_progress'; save(QUEUE,load(QUEUE,{}))
    aider=find_aider()
    state.update({'current_task':task['id'],'task_id':task['id'],'phase':'execute','executor_order':EXECUTORS,'optional_backends':['openhands'],'why_waiting':None,'aider_executable':aider,'aider_status':'available' if aider else 'missing','openhands_status':'optional-not-probed','litellm_status':'not-required'})
    update(state); model,model_probe=choose_model(state,model_order)
    if not model:
        state.update({'phase':'waiting','why_waiting':'all Ollama models unavailable'}); checkpoint(state,task,'model failover exhausted','retry Ollama models',{'last_probe':model_probe}); update(state); return 2
    task_prompt=prompt or continuation_context(task,state)
    for executor in EXECUTORS:
        state.update({'phase':'execute','executor':executor,'model':model}); update(state); recovered=acquire(executor,task)
        if recovered: state['stale_lock_recovered_at']=now(); update(state)
        try:
            if executor=='codex' and simulate_codex_unavailable: result={'executor':'codex','exit_code':75,'duration_seconds':0,'output':'simulated codex unavailable'}
            else:
                try:
                    env=os.environ.copy(); env['OLLAMA_API_BASE']=OLLAMA_URL
                    result=run(executor_cmd(executor,task_prompt,model),executor,env=env)
                except (FileNotFoundError,OSError) as exc: result={'executor':executor,'exit_code':127,'duration_seconds':0,'output':str(exc)}
            kind='ok' if result['exit_code']==0 and result['output'].strip() else failure_kind(result['output'])
            state.setdefault('executor_attempts',{})[executor]=state.get('executor_attempts',{}).get(executor,0)+1
            checkpoint(state,task,f'{executor}: {kind}','tests' if kind=='ok' else 'executor failover',{'result':result,'model_probe':model_probe}); update(state)
            if kind!='ok':
                state.setdefault('executor_failures',{})[executor]=state.get('executor_failures',{}).get(executor,0)+1
                if executor=='codex': state['codex_available']=False; state['codex_retry_after']=retry_after(result['output'])
                update(state); continue
        finally: release()
        if probe_file:
            path=ROOT/probe_file
            actual=path.read_text(encoding='utf-8').strip() if path.is_file() else None
            if actual!=probe_contents:
                state.update({'phase':'waiting','why_waiting':f'executor probe verification failed: {probe_file}'})
                checkpoint(state,task,'executor output verification failed','retry executor probe',{'expected':probe_contents,'actual':actual}); update(state); return 5
        state['phase']='test'
        if run_tests(state)['exit_code']!=0: checkpoint(state,task,'tests failed','fix tests'); return 3
        state['phase']='review'; review_result=review(state,task)
        passed=review_result['exit_code']==0 and review_result['response'].lstrip().upper().startswith('PASS')
        checkpoint(state,task,'review passed' if passed else 'review failed','complete' if passed and complete else 'await completion',{'review':review_result})
        if not passed: state.update({'phase':'waiting','why_waiting':'reviewer did not pass'}); update(state); return 4
        state.update({'phase':'validated','why_waiting':None if complete else 'validated; completion not requested'})
        if complete: mark_done(task); state['phase']='complete'
        update(state); return 0
    state.update({'phase':'waiting','why_waiting':'all active executors failed'}); checkpoint(state,task,'executor failover exhausted','retry active executors'); update(state); return 2
def main():
    p=argparse.ArgumentParser(); p.add_argument('--once',action='store_true'); p.add_argument('--daemon',action='store_true'); p.add_argument('--status',action='store_true'); p.add_argument('--simulate-codex-unavailable',action='store_true'); p.add_argument('--prompt'); p.add_argument('--complete-current',action='store_true'); p.add_argument('--probe-file'); p.add_argument('--probe-contents'); p.add_argument('--recover-lock-probe',action='store_true'); p.add_argument('--watchdog-probe'); a=p.parse_args()
    if a.status: print(json.dumps(load(STATE,{}),indent=2)); return 0
    if a.recover_lock_probe: return recover_lock_probe()
    if a.watchdog_probe: return watchdog_probe(a.watchdog_probe)
    if a.daemon:
        while True:
            try: run_cycle(); time.sleep(30)
            except KeyboardInterrupt: return 130
    return run_cycle(a.simulate_codex_unavailable,a.prompt,a.complete_current,probe_file=a.probe_file,probe_contents=a.probe_contents)
if __name__=='__main__': raise SystemExit(main())
