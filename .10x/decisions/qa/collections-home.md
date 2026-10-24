# QA — collections-home

New/extended: tst_librarymigration (v2 -> v3, v1 -> v3), tst_gamelibrary (collections, pinning, play time persisted), tst_settings (home.sections), tst_uicontext (collections/pinning/Home layout, play time excludes pauses), tst_qmlsmoke::collectionsPinningAndHomeLayout (both resolutions). 44/44 on GCC and Clang (-Werror), qmllint 0 errors.

Found and fixed: the intermittent tst_qmlsmoke failure reported on PR #21 — a deferred Main.restoreFocus() could take focus from the "Can't resume" dialog under load (ef9d1ea); 6/6 runs under parallel load pass.
