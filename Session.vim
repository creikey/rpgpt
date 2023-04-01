let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/Documents/rpgpt
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +1 codegen.c
badd +1 main.c
badd +1 maketraining.c
badd +1 thirdparty/md.c
badd +1 assets.mdesk
badd +1 elements.mdesk
badd +1 gen/assets.gen.c
badd +1 gen/training_data.jsonl
badd +1 server/main.go
badd +1 server/codes/codes.go
badd +1 server/codes/codes_test.go
badd +1 server/rpgpt.service
badd +1 server/install_service.sh
badd +1 API_KEY.bat
badd +1 build_desktop_debug.bat
badd +1 build_web_debug.bat
badd +1 build_web_release.bat
badd +1 run_codegen.bat
badd +1 buff.h
badd +1 makeprompt.h
badd +1 profiling.h
badd +1 thirdparty/dr_wav.h
badd +1 thirdparty/HandmadeMath.h
badd +1 thirdparty/md.h
badd +1 thirdparty/md_stb_sprintf.h
badd +1 thirdparty/sokol_app.h
badd +1 thirdparty/sokol_audio.h
badd +1 thirdparty/sokol_gfx.h
badd +1 thirdparty/sokol_glue.h
badd +1 thirdparty/sokol_log.h
badd +1 thirdparty/sokol_time.h
badd +1 thirdparty/spall.h
badd +1 thirdparty/stb_image.h
badd +1 thirdparty/stb_truetype.h
badd +1 gen/characters.gen.h
badd +1 gen/quad-sapp.glsl.h
argglobal
%argdel
$argadd codegen.c
$argadd main.c
$argadd maketraining.c
$argadd thirdparty/md.c
$argadd assets.mdesk
$argadd elements.mdesk
$argadd gen/assets.gen.c
$argadd gen/training_data.jsonl
$argadd server/main.go
$argadd server/codes/codes.go
$argadd server/codes/codes_test.go
$argadd server/rpgpt.service
$argadd server/install_service.sh
$argadd API_KEY.bat
$argadd build_desktop_debug.bat
$argadd build_web_debug.bat
$argadd build_web_release.bat
$argadd run_codegen.bat
$argadd buff.h
$argadd makeprompt.h
$argadd profiling.h
$argadd thirdparty/dr_wav.h
$argadd thirdparty/HandmadeMath.h
$argadd thirdparty/md.h
$argadd thirdparty/md_stb_sprintf.h
$argadd thirdparty/sokol_app.h
$argadd thirdparty/sokol_audio.h
$argadd thirdparty/sokol_gfx.h
$argadd thirdparty/sokol_glue.h
$argadd thirdparty/sokol_log.h
$argadd thirdparty/sokol_time.h
$argadd thirdparty/spall.h
$argadd thirdparty/stb_image.h
$argadd thirdparty/stb_truetype.h
$argadd gen/characters.gen.h
$argadd gen/quad-sapp.glsl.h
edit main.c
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 104 + 104) / 209)
exe 'vert 2resize ' . ((&columns * 104 + 104) / 209)
argglobal
if bufexists(fnamemodify("main.c", ":p")) | buffer main.c | else | edit main.c | endif
if &buftype ==# 'terminal'
  silent file main.c
endif
balt codegen.c
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1 - ((0 * winheight(0) + 24) / 49)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1
normal! 0
wincmd w
argglobal
if bufexists(fnamemodify("makeprompt.h", ":p")) | buffer makeprompt.h | else | edit makeprompt.h | endif
if &buftype ==# 'terminal'
  silent file makeprompt.h
endif
balt codegen.c
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1 - ((0 * winheight(0) + 24) / 49)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1
normal! 0
wincmd w
2wincmd w
exe 'vert 1resize ' . ((&columns * 104 + 104) / 209)
exe 'vert 2resize ' . ((&columns * 104 + 104) / 209)
tabnext 1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
let &shortmess = s:shortmess_save
let &winminheight = s:save_winminheight
let &winminwidth = s:save_winminwidth
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
set hlsearch
nohlsearch
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
