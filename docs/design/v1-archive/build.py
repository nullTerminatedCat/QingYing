import random
scr=open('screen.inc').read()
def page(name,body,extra_css=''):
    open(name,'w').write(f'<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><link rel="stylesheet" href="common.css"><style>{extra_css}</style></head><body>{body}<script src="icons.js"></script></body></html>')
def handles(x,y,w,h):
    return ''.join(f'<div class="handle" style="left:{x+dx-4}px;top:{y+dy-4}px"></div>' for dx in (0,w/2,w) for dy in (0,h/2,h) if not(dx==w/2 and dy==h/2))
def sel(x,y,w,h,dash=False):
    b='2px dashed' if dash else '1.5px solid'
    return f'<div style="position:absolute;left:{x}px;top:{y}px;width:{w}px;height:{h}px;border:{b} var(--p2);box-shadow:0 0 0 3000px rgba(10,20,18,.48)"></div>'
def magnifier(x,y):
    random.seed(3);cells=''
    for r in range(15):
        for c in range(15):
            col='#ffffff' if r<6 else ('#167468' if 4<c<10 else '#e4ece9')
            if r==6: col='#dfe7e4'
            cells+=f'<div style="background:{col}"></div>'
    return f'''<div style="position:absolute;left:{x}px;top:{y}px;width:150px;background:#fff;border-radius:8px;box-shadow:0 6px 20px rgba(0,0,0,.35);overflow:hidden;border:1px solid var(--bd)">
<div style="position:relative;display:grid;grid-template-columns:repeat(15,10px);width:150px;height:150px;gap:0">{cells}
<div style="position:absolute;inset:0;background-image:linear-gradient(rgba(0,0,0,.08) 1px,transparent 1px),linear-gradient(90deg,rgba(0,0,0,.08) 1px,transparent 1px);background-size:10px 10px"></div>
<div style="position:absolute;left:70px;top:0;width:10px;height:150px;background:rgba(22,116,104,.25)"></div><div style="position:absolute;top:70px;left:0;height:10px;width:150px;background:rgba(22,116,104,.25)"></div>
<div style="position:absolute;left:69px;top:69px;width:12px;height:12px;border:2px solid #20332f"></div></div>
<div style="padding:7px 9px;font:12px Consolas,monospace;line-height:1.6;color:var(--t)"><div>坐标 <b>(1046, 452)</b></div><div style="display:flex;align-items:center;gap:6px"><span style="width:12px;height:12px;background:#167468;border-radius:3px;display:inline-block"></span>RGB 22,116,104</div><div>HEX <b style="color:var(--p)">#167468</b></div>
<div style="color:var(--t3);font-family:inherit;font-size:11px;border-top:1px solid var(--bd);margin-top:4px;padding-top:4px">C 复制色值 · Shift 切换格式</div></div></div>'''
hint='<div class="tip" style="left:50%;transform:translateX(-50%);top:16px;padding:7px 14px;background:rgba(32,51,47,.9)">拖动选择区域 · 单击确认窗口 · 方向键微调 · Esc 退出</div>'
# 01a window detect
page('01a-window-detect.html',scr+sel(760,130,580,460,dash=False)+'<div style="position:absolute;left:760px;top:130px;width:580px;height:460px;background:rgba(22,116,104,.10);border:2px solid #4aa18f"></div>'
 +'<div class="label" style="left:760px;top:104px">文档 - 编辑器 · 580 × 460</div>'+hint+magnifier(1090,600-0))
# 01b region
X,Y,W,H=820,250,460,300
page('01b-region-select.html',scr+sel(X,Y,W,H)+handles(X,Y,W,H)+f'<div class="label" style="left:{X}px;top:{Y-26}px">{W} × {H}</div>'+hint+magnifier(X+W-136,Y+H+16)
 +f'<div style="position:absolute;left:{X+W-6}px;top:{Y+H-6}px;width:14px;height:14px"><svg viewBox="0 0 14 14" width="14" height="14"><path d="M7 0v14M0 7h14" stroke="#fff" stroke-width="1.5"/></svg></div>')
def toolbar(x,y,active='rect'):
    tools=[('rect','矩形'),('ellipse','椭圆'),('arrow','箭头'),('pen','画笔'),('text','文字'),('mosaic','马赛克'),('num','序号')]
    h=''.join(f'<div class="tbtn {"on" if t==active else ""}" data-i="{t}" title="{n}"></div>' for t,n in tools)
    h+='<div class="sep"></div><div class="tbtn" data-i="undo"></div><div class="tbtn dis" data-i="redo"></div><div class="sep"></div><div class="tbtn" data-i="pin"></div><div class="tbtn" data-i="save"></div><div class="tbtn x" data-i="close"></div><div class="tbtn ok" data-i="copy"></div>'
    return f'<div class="toolbar" style="left:{x}px;top:{y}px">{h}</div>'
def stylebar(x,y,font=False,cur='#e5484d'):
    cols=['#e5484d','#f5a524','#f2d12f','#2fbf71','#167468','#2f80ed','#8e5cf7','#20332f','#ffffff']
    sw=''.join(f'<span style="width:20px;height:20px;border-radius:5px;background:{c};border:{"2px solid #fff;box-shadow:0 0 0 2px #167468" if c==cur else "1px solid rgba(0,0,0,.12)"};display:inline-block"></span>' for c in cols)
    ws=''.join(f'<span class="tbtn {"on" if i==1 else ""}" style="width:28px;height:28px"><span style="width:{s}px;height:{s}px;border-radius:50%;background:currentColor;display:block"></span></span>' for i,s in enumerate((4,7,11)))
    fz=('<div class="sep"></div><span style="color:var(--t2);font-size:12px">字号</span><span style="border:1px solid var(--bd);border-radius:5px;padding:3px 8px;font-size:12px;min-width:56px;display:inline-flex;justify-content:space-between">16 <span style="color:var(--t3)">▾</span></span><span class="tbtn on" style="width:28px;height:28px;font-weight:700">B</span>' if font else '')
    return f'<div class="toolbar" style="left:{x}px;top:{y}px;gap:6px;padding:6px 10px">{ws}<div class="sep"></div>{sw}<div class="sep"></div><span style="width:20px;height:20px;border-radius:5px;background:conic-gradient(red,yellow,lime,cyan,blue,magenta,red);display:inline-block"></span>{fz}</div>'
# 02 toolbar
X,Y,W,H=820,180,460,300
tips='<div class="tip" style="left:%dpx;top:%dpx">矩形 (R)</div>'%(X+W-470+2,Y+H+64)
page('02-annotation-toolbar.html',scr+sel(X,Y,W,H)+handles(X,Y,W,H)+f'<div class="label" style="left:{X}px;top:{Y-26}px">{W} × {H}</div>'
 +toolbar(X+W-536,Y+H+8)+stylebar(X+W-536,Y+H+58,font=True)
 # spec callouts
 +'''<div style="position:absolute;left:40px;top:640px;width:700px;background:#fff;border-radius:10px;padding:16px 20px;box-shadow:0 8px 30px rgba(0,0,0,.25);font-size:12px;line-height:1.9">
<div style="font-size:15px;font-weight:600;margin-bottom:6px;color:var(--pdd)">工具条说明</div>
<div>标注：矩形 R · 椭圆 E · 箭头 A · 画笔 P · 文字 T · 马赛克 M · 序号 N</div>
<div>操作：撤销 Ctrl+Z · 重做 Ctrl+Y · 钉图 F3 · 保存 Ctrl+S · 取消 Esc · <b style="color:var(--p)">复制并完成 Enter / 双击</b></div>
<div>二级样式条：粗细 3 档 · 9 色预设 + 自定义 · 文字工具时显示字号与加粗</div>
<div>按钮状态：<span class="tbtn" style="display:inline-flex;vertical-align:middle;background:var(--bgt)" data-i="rect"></span> 悬停 <span class="tbtn on" style="display:inline-flex;vertical-align:middle" data-i="rect"></span> 选中 <span class="tbtn dis" style="display:inline-flex;vertical-align:middle" data-i="redo"></span> 禁用 <span class="tbtn ok" style="display:inline-flex;vertical-align:middle" data-i="copy"></span> 主操作</div></div>''')
# 03 annotated
X,Y,W,H=780,150,560,420
ann=f'''<svg style="position:absolute;left:0;top:0" width="1440" height="900">
<rect x="830" y="290" width="160" height="120" fill="none" stroke="#e5484d" stroke-width="3" rx="2"/>
<ellipse cx="1230" cy="330" rx="70" ry="38" fill="none" stroke="#f5a524" stroke-width="3"/>
<defs><marker id="ah" markerWidth="10" markerHeight="10" refX="6" refY="5" orient="auto"><path d="M0 0L10 5L0 10z" fill="#e5484d"/></marker></defs>
<path d="M1120 500 L1010 400" stroke="#e5484d" stroke-width="4" marker-end="url(#ah)"/>
<path d="M1060 240 q20 -18 40 0 t40 0 t40 0" fill="none" stroke="#2fbf71" stroke-width="3" stroke-linecap="round"/>
</svg>
<div style="position:absolute;left:1120px;top:500px;color:#e5484d;font-size:16px;font-weight:700;border:1px dashed #e5484d;padding:2px 6px">Q4 增长最快</div>
<div style="position:absolute;left:840px;top:205px;width:150px;height:20px;background:repeating-conic-gradient(#b8c7c2 0 25%,#dfe7e4 0 50%) 0 0/8px 8px"></div>
''' + ''.join(f'<div style="position:absolute;left:{x}px;top:{y}px;width:24px;height:24px;border-radius:50%;background:#e5484d;color:#fff;font-weight:700;display:flex;align-items:center;justify-content:center;font-size:13px;box-shadow:0 0 0 2px #fff">{i}</div>' for i,(x,y) in enumerate([(818,278),(1290,300),(1110,470)],1))
page('03-annotated-example.html',scr+sel(X,Y,W,H)+ann+f'<div class="label" style="left:{X}px;top:{Y-26}px">{W} × {H}</div>'+toolbar(X+W-536,Y+H+8,'text')+stylebar(X+W-536,Y+H+58,font=True)
 +'<div class="tip" style="left:840px;top:180px">马赛克</div>')
# 04 pinned
pin='''<div style="position:absolute;left:520px;top:200px;width:420px;height:300px;border-radius:4px;box-shadow:0 0 0 1px #4aa18f,0 12px 36px rgba(0,0,0,.45);overflow:hidden;background:#fff">
<div style="padding:20px"><div style="font-size:16px;font-weight:600">季度报告</div><div class="line" style="margin:10px 0"></div><div class="line" style="margin:10px 0;width:80%"></div>
<div style="display:flex;gap:10px;align-items:flex-end;height:150px;margin-top:16px"><div style="flex:1;height:60%;background:#4aa18f"></div><div style="flex:1;height:85%;background:#167468"></div><div style="flex:1;height:45%;background:#8cbbae"></div><div style="flex:1;height:100%;background:#0e6258"></div></div></div>
<div class="toolbar" style="right:8px;top:8px;padding:3px;box-shadow:0 2px 8px rgba(0,0,0,.2)"><div class="tbtn" style="width:28px;height:28px" data-i="crop"></div><div class="tbtn" style="width:28px;height:28px" data-i="copy"></div><div class="tbtn" style="width:28px;height:28px" data-i="save"></div><div class="tbtn x" style="width:28px;height:28px" data-i="close"></div></div>
<div class="label" style="left:8px;bottom:8px">100% · 不透明度 100%</div></div>
<div style="position:absolute;left:960px;top:220px;width:200px;background:#fff;border-radius:8px;box-shadow:0 8px 24px rgba(0,0,0,.3);padding:5px;font-size:13px">
''' + ''.join(f'<div style="display:flex;align-items:center;gap:8px;padding:6px 10px;border-radius:5px;{"background:var(--bgt);color:var(--p)" if i==1 else ""}"><span data-i="{ic}" style="display:flex;color:var(--t2)"></span><span style="flex:1">{t}</span><span style="color:var(--t3);font-size:11px">{k}</span></div>' + ('<div style="height:1px;background:var(--bd);margin:4px 0"></div>' if i in (2,5) else '') for i,(ic,t,k) in enumerate([('copy','复制','Ctrl+C'),('save','另存为…','Ctrl+S'),('crop','编辑标注',''),('plus','放大','滚轮'),('opacity','不透明度','Ctrl+滚轮'),('history','恢复原始大小','1'),('close','关闭钉图','Esc')])) + '</div>'
pin2='<div style="position:absolute;left:150px;top:620px;width:240px;height:140px;border-radius:4px;box-shadow:0 10px 30px rgba(0,0,0,.4);background:#edf6f2;opacity:.6;border:1px solid #fff"><div style="margin:20px;height:90px;border:2px dashed #8abbb0;border-radius:8px"></div></div><div class="label" style="left:150px;top:766px">不透明度 60%（未悬停）</div>'
page('04-pinned-window.html','<div class="desk"></div><div class="win" style="left:120px;top:80px;width:1180px;height:720px"><div class="tb">● 桌面应用</div><div class="line" style="width:40%;margin-top:30px"></div><div class="line" style="width:60%"></div><div class="line" style="width:50%"></div></div><div class="taskbar"><span>09:33<br>2026/10/21</span></div>'+pin+pin2+'<div class="cap" style="color:#20332f;text-shadow:none;left:150px;top:96px;background:#fff;padding:4px 0">钉图：悬停显示工具条与缩放信息 · 右键菜单 · 拖动移动 · 滚轮缩放 · 双击关闭</div>')
# 05 tray + settings
menu='<div style="position:absolute;right:20px;bottom:54px;width:230px;background:#fff;border-radius:8px;box-shadow:0 8px 28px rgba(0,0,0,.35);padding:6px;font-size:13px">'+'<div style="padding:8px 10px;display:flex;align-items:center;gap:8px;border-bottom:1px solid var(--bd);margin-bottom:4px"><span style="width:22px;height:22px;border-radius:6px;background:var(--p);color:#fff;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:12px">影</span><b>青影 QingYing</b><span style="color:var(--t3);font-size:11px;margin-left:auto">v0.1.0</span></div>'+''.join(f'<div style="display:flex;align-items:center;gap:8px;padding:7px 10px;border-radius:5px;{"background:var(--bgt);color:var(--p)" if i==0 else ""}"><span data-i="{ic}" style="display:flex;color:{"var(--p)" if i==0 else "var(--t2)"}"></span><span style="flex:1">{t}</span><span style="color:var(--t3);font-size:11px">{k}</span></div>'+('<div style="height:1px;background:var(--bd);margin:4px 0"></div>' if i in (2,4) else '') for i,(ic,t,k) in enumerate([('crop','区域截图','F1'),('screen','全屏截图','Ctrl+F1'),('pin','贴图剪贴板','F3'),('folder','打开保存目录',''),('gear','设置…',''),('info','关于',''),('power','退出','')]))+'</div>'
def row(lbl,ctrl): return f'<div style="display:flex;align-items:center;padding:10px 0;border-bottom:1px solid #e4ece9"><div style="width:110px;color:var(--t2)">{lbl}</div><div style="flex:1;display:flex;gap:8px;align-items:center">{ctrl}</div></div>'
inp=lambda v,w=200,mono=False:f'<span style="display:inline-flex;align-items:center;height:30px;padding:0 10px;border:1px solid var(--bd2);border-radius:6px;width:{w}px;background:#fff;{"font-family:Consolas,monospace" if mono else ""}">{v}</span>'
btn=lambda t,p=False:f'<span style="height:30px;padding:0 14px;border-radius:6px;display:inline-flex;align-items:center;{"background:var(--p);color:#fff" if p else "border:1px solid var(--bd2);background:#fff"}">{t}</span>'
sw=lambda on:f'<span style="width:36px;height:20px;border-radius:10px;background:{"var(--p)" if on else "#c2d8d1"};position:relative;display:inline-block"><span style="position:absolute;top:2px;{"right" if on else "left"}:2px;width:16px;height:16px;border-radius:50%;background:#fff"></span></span>'
radio=lambda t,on:f'<span style="display:inline-flex;align-items:center;gap:5px;margin-right:10px"><span style="width:14px;height:14px;border-radius:50%;border:{"4px solid var(--p)" if on else "1.5px solid #9aada7"}"></span>{t}</span>'
settings=f'''<div class="win" style="left:330px;top:120px;width:620px;height:520px;border:1px solid var(--bd)">
<div style="height:40px;display:flex;align-items:center;padding:0 14px;gap:8px;border-bottom:1px solid var(--bd)"><span style="width:18px;height:18px;border-radius:5px;background:var(--p);color:#fff;font-size:10px;display:flex;align-items:center;justify-content:center;font-weight:700">影</span>青影 设置<span style="margin-left:auto;color:var(--t2)" data-i="close"></span></div>
<div style="display:flex;height:440px"><div style="width:140px;background:#f6fbf9;border-right:1px solid var(--bd);padding:10px 8px">{''.join(f'<div style="padding:8px 12px;border-radius:6px;margin-bottom:2px;{"background:var(--sel);color:var(--p);font-weight:600" if i==0 else "color:var(--t2)"}">{t}</div>' for i,t in enumerate(['常规','快捷键','保存','关于']))}</div>
<div style="flex:1;padding:14px 24px">
<div style="font-size:15px;font-weight:600;margin-bottom:4px">常规</div>
{row('区域截图',inp('<span style="background:var(--bgt);color:var(--p);padding:1px 6px;border-radius:4px">F1</span>',150,True)+btn('修改'))}
{row('全屏截图',inp('<span style="border:1.5px solid var(--p);color:var(--p);padding:0 6px;border-radius:4px">请按下新快捷键…</span>',150)+'<span style="color:#d9534f;font-size:12px">Ctrl+F1 已被占用</span>')}
{row('保存路径',inp('D:\\Pictures\\QingYing',220)+btn('浏览…'))}
{row('文件名',inp('QingYing_yyyyMMdd_HHmmss',220,True))}
{row('图片格式',radio('PNG',True)+radio('JPG',False)+'<span style="color:var(--t3);font-size:12px">JPG 质量 90</span>')}
{row('开机自启',sw(True)+'<span style="color:var(--t2);font-size:12px">登录 Windows 时启动并常驻托盘</span>')}
{row('截图后',sw(False)+'<span style="color:var(--t2);font-size:12px">自动保存到文件</span>')}
<div style="display:flex;justify-content:flex-end;gap:8px;margin-top:22px">{btn('恢复默认')}{btn('取消')}{btn('保存',True)}</div></div></div></div>'''
page('05-tray-settings.html','<div class="desk"></div>'+settings+menu+'<div class="taskbar"><span>中</span><span style="outline:2px solid #4aa18f;border-radius:4px;padding:2px;background:rgba(255,255,255,.1)"><span style="display:inline-block;width:16px;height:16px;background:#167468;border-radius:4px;color:#fff;font-size:10px;text-align:center;line-height:16px">影</span></span><span>09:33<br>2026/10/21</span></div>')
