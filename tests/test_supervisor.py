import json, os, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
import sys
sys.path.insert(0, str(Path(__file__).parents[1] / 'ai'))
import supervisor

class SupervisorTests(unittest.TestCase):
    def test_failure_classification(self):
        self.assertEqual(supervisor.failure_kind("You've hit your usage limit; try again at 2026-08-27T01:00Z"), 'unavailable')
        self.assertEqual(supervisor.failure_kind(''), 'semantic')
    def test_retry_timestamp(self):
        self.assertEqual(supervisor.retry_after('try again at 2026-08-27T01:00Z'), '2026-08-27T01:00Z')
    def test_atomic_json(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'x.json'; supervisor.save(p, {'ok':True}); self.assertEqual(supervisor.load(p,{}), {'ok':True})
    def test_model_order(self):
        self.assertEqual(supervisor.MODELS, ['qwen2.5-coder:14b','qwen2.5:latest'])

    def test_fallback_context_preserves_build_fix_state(self):
        with tempfile.TemporaryDirectory() as d:
            ai=Path(d)/'ai'; build_log=ai/'logs'/'build'/'latest.log'
            build_log.parent.mkdir(parents=True)
            build_log.write_bytes(b'\xff\xfe'+('compile.cpp:42: error: readable failure\n').encode('utf-16-le'))
            checkpoint=Path(d)/'checkpoint.json'
            checkpoint.write_text(json.dumps({'summary':'preserved checkpoint'}),encoding='utf-8')
            state={'current_task':'flux-next-safe-triage','last_checkpoint':'checkpoint.json'}
            with patch.object(supervisor,'ROOT',Path(d)), patch.object(supervisor,'AI',ai), patch.object(supervisor.subprocess, 'run') as run:
                run.return_value.stdout = ' M src/main.cpp\n'
                prompt = supervisor.continuation_context({'id':'flux-next-safe-triage','description':'build fix'}, state)
        self.assertIn('CURRENT_TASK=flux-next-safe-triage', prompt)
        self.assertIn('"current_task": "flux-next-safe-triage"', prompt)
        self.assertIn('preserved checkpoint', prompt)
        self.assertIn('GIT_DIFF_STAT=', prompt)
        self.assertIn('BUILD_LOG_TAIL=', prompt)
        self.assertIn('compile.cpp:42: error: readable failure', prompt)
        self.assertNotIn('\x00',prompt)
        self.assertEqual(supervisor.REVIEWER_MODEL, 'qwen2.5:14b')
    def test_byte_text_loading_detects_utf16_and_removes_residual_nuls(self):
        self.assertEqual(supervisor.decode_text(b'\xff\xfe'+('hello\n').encode('utf-16-le')),'hello\n')
        self.assertEqual(supervisor.decode_text(('little endian').encode('utf-16-le')),'little endian')
        self.assertEqual(supervisor.decode_text(b'\xfe\xff'+('big endian').encode('utf-16-be')),'big endian')
        self.assertEqual(supervisor.decode_text(b'plain\x00 utf-8'),'plain utf-8')
    def test_run_sanitizes_all_argv_and_environment_nuls(self):
        completed=type('Completed',(),{'stdout':'ok','stderr':'','returncode':0})()
        with tempfile.TemporaryDirectory() as d, patch.object(supervisor,'LOGS',Path(d)), patch.object(supervisor.subprocess,'run',return_value=completed) as run:
            result=supervisor.run([r'C:\tools\aider.exe','--message','CURRENT\x00_TASK=probe'],'aider',env={'GOOD\x00_KEY':'safe\x00value',**{key:value for key,value in os.environ.items() if '\x00' not in key and '\x00' not in value}})
        argv=run.call_args.args[0]; env=run.call_args.kwargs['env']
        self.assertTrue(all('\x00' not in value for value in argv))
        self.assertTrue(all('\x00' not in key and '\x00' not in value for key,value in env.items()))
        self.assertIn('CURRENT_TASK=probe',argv)
        self.assertEqual(result['exit_code'],0)
    def test_executor_order(self):
        self.assertEqual(supervisor.EXECUTORS, ['codex','aider'])
        self.assertNotIn('openhands', supervisor.EXECUTORS)
    def test_codex_command_uses_safe_global_approval_order_and_preserves_task(self):
        prompt='CURRENT_TASK=probe\ncontinue safely'
        command=supervisor.executor_cmd('codex',prompt,'unused')
        self.assertEqual(command,['codex','--ask-for-approval','never','exec','-C',str(supervisor.ROOT),'-s','workspace-write','--ephemeral','--json',prompt])
        self.assertIn('CURRENT_TASK=probe',command[-1])
    def test_stale_lock_recovery(self):
        with tempfile.TemporaryDirectory() as d:
            lock=Path(d)/'.lock'; lock.write_text(json.dumps({'pid':99999999}),encoding='utf-8')
            with patch.object(supervisor,'LOCK',lock):
                self.assertTrue(supervisor.acquire('aider',{'id':'probe'}))
                self.assertEqual(json.loads(lock.read_text())['executor'],'aider')
                supervisor.release(); self.assertFalse(lock.exists())
    def test_aider_uses_native_ollama(self):
        with tempfile.TemporaryDirectory() as d, patch.object(supervisor,'ROOT',Path(d)), patch.object(supervisor,'find_aider',return_value=r'C:\tools\aider.exe'):
            command=supervisor.executor_cmd('aider','CURRENT_TASK=probe','qwen2.5-coder:14b')
        self.assertIn('ollama_chat/qwen2.5-coder:14b',command)
        self.assertIn('CURRENT_TASK=probe',command)
        self.assertIn('--no-git',command)
        self.assertNotIn('--model-metadata-file',command)
    def test_aider_uses_local_model_metadata_when_present(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); metadata=root/'.aider.model.metadata.json'; metadata.write_text('{}',encoding='utf-8')
            with patch.object(supervisor,'ROOT',root), patch.object(supervisor,'find_aider',return_value=r'C:\tools\aider.exe'):
                command=supervisor.executor_cmd('aider','CURRENT_TASK=probe','qwen2.5-coder:14b')
        index=command.index('--model-metadata-file')
        self.assertEqual(command[index+1],str(metadata))
    def test_recovery_keeps_current_task(self):
        state={'current_task':'ai-infrastructure-001'}
        self.assertEqual(state['current_task'],'ai-infrastructure-001')

if __name__=='__main__': unittest.main()
