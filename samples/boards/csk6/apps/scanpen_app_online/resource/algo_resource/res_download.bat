@echo on

:: 构建lfs1文件系统
::del /f /s /q D:\work\venus\software\lscloud\zephyr\samples\boards\csk6\apps\scanpen_ai\resource\lfs1\zephyr.bin
::mklfs.exe .\zephyr\samples\boards\csk6\apps\scanpen_ai\resource\lfs1 .\lfs1.bin 0x2a0000
::C:\Users\yhzhu\.listenai\jlink-env\1.0.3\JFlash.exe -openprjC:\Users\yhzhu\.listenai\jlink-env\1.0.3\Default.jflash -openD:\work\venus\software\lscloud\lfs1.bin -relocate0x18260000 -auto -exit

:: 烧录资源
set JFLASH=C:\Users\yhzhu\.listenai\jlink-env\1.0.3\JFlash.exe
set CONFIG=C:\Users\yhzhu\.listenai\jlink-env\1.0.3\Default.jflash
set RES_DIR=D:\work\venus\software\lscloud\zephyr\samples\boards\csk6\apps\scanpen_app_online\resource\algo_resource

:: cp.bin
%JFLASH% -openprj%CONFIG% -open%RES_DIR%\zephyr.bin -relocate0x18500000 -auto -exit

:: cutline
%JFLASH% -openprj%CONFIG% -open%RES_DIR%\mlp_cutline_castor.bin -relocate0x18800000 -auto -exit

:: ocr
%JFLASH% -openprj%CONFIG% -open%RES_DIR%\venus_ocr_ctc_v5.bin -relocate0x18880000 -auto -exit

:: tts
%JFLASH% -openprj%CONFIG% -open%RES_DIR%\ivTTS_CE+PE_Castor_xiaoyan_v3.16K.bin -relocate0x18b00000 -auto -exit

:: translate
%JFLASH% -openprj%CONFIG% -open%RES_DIR%\translate.bin -relocate0x18f80000 -auto -exit

::pause