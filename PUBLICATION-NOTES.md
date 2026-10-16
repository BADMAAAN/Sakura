# SAKURA — состав публикации

Основа — точный коммит, указанный в PUBLICATION-PROVENANCE.json. Снимок обновлён с keyboard/power hotfix и Sakura Tamagotchi; исходники прошивки, зависимости, аппаратная конфигурация, LICENSE и THIRD_PARTY сохранены. Изменения этого этапа относятся к README, презентационной графике и документации/конфигурации GitHub.

Первый push этого проверенного состава согласован текущим заданием владельца. Удалённые refs повторно проверяются перед обычным push; существующая удалённая история не переписывается. Отдельная история публикационного снимка сохраняет provenance исходной базы; старый FLUX и локальная история разработки остаются нетронутыми. Force push, release, firmware-бинарники и полный ZIP не входят в этот этап.

Унаследованные workflows и FUNDING архивированы в docs/upstream-workflows с исходными credits. Автоматическое создание релизов, публикация бинарников и закрытие issues не включены. Девять generated logs и optional media/pictures/nfc.gif исключены; проверенные унаследованные ZIP ресурсов сохранены. Приватные конфигурации, личные фотографии, дампы, кэши и toolchain не публикуются.

PUBLICATION-SHA256SUMS покрывает все публичные файлы, кроме самого manifest. Локальный .sakura-publication-export.json и .git не входят в состав. После оформления старый exporter поверх снимка не запускается: его защита запрещает перезапись изменённых файлов. Результаты и границы проверок приведены в docs/PUBLICATION_CHECKS.md; происхождение графики — в docs/assets/provenance.json.

This publication snapshot preserves the exact reviewed firmware source and attribution. The current task authorizes the verified first normal push. Inherited release workflows are archived; no firmware binaries, complete source archive or release are published. Source validation, host-image provenance and clean-checkout test limitations are documented separately. Existing remote history is preserved if it appears before push.
