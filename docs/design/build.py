S=open('scene.inc').read()
def page(n,b): open(n,'w').write(f'<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><link rel="stylesheet" href="common.css"></head><body>{b}<script src="icons.js"></script></body></html>')
def sel(x,y,w,h): return f'<div style="position:absolute;left:{x}px;top:{y}px;width:{w}px;height:{h}px;border-radius:2px;box-shadow:0 0 0 1.5px var(--a),0 0 0 3000px rgba(15,18,30,.42)"></div>'+''.join(f'<div style="position:absolute;left:{x+dx-5}px;top:{y+dy-5}px;width:10px;height:10px;border-radius:50%;background:#fff;box-shadow:0 0 0 1.5px var(--a),0 1px 3px rgba(0,0,0,.3)"></div>' for dx,dy in ((0,0),(w,0),(0,h),(w,h)))
def size(x,y,t): return f'<div class="pill" style="left:{x}px;top:{y}px">{t}</div>'
def mag(x,y):
    c=''.join(f'<div style="background:{"#ffffff" if r<5 else ("#4F6BFF" if 3<k<9 else "#eceef3")}"></div>' for r in range(13) for k in range(13))
    return f'''<div style="position:absolute;left:{x}px;top:{y}px;width:132px">
<div style="position:relative;width:132px;height:132px;border-radius:50%;overflow:hidden;box-shadow:0 0 0 3px #fff,0 10px 30px rgba(0,0,0,.3);display:grid;grid-template-columns:repeat(13,1fr)">{c}
<div style="position:absolute;inset:0;background-image:linear-gradient(rgba(0,0,0,.06) 1px,transparent 1px),linear-gradient(90deg,rgba(0,0,0,.06) 1px,transparent 1px);background-size:10.15px 10.15px"></div>
<div style="position:absolute;left:60px;top:60px;width:12px;height:12px;border:1.5px solid #1c1c1e;border-radius:2px;box-shadow:0 0 0 1px #fff"></div></div>
<div class="pill" style="position:relative;margin:10px auto 0;width:max-content;display:flex;gap:8px;align-items:center;font-family:Consolas,monospace"><i style="width:10px;height:10px;border-radius:3px;background:#4F6BFF;box-shadow:0 0 0 1px #fff"></i>#4F6BFF<span style="opacity:.6">79,107,255</span></div></div>'''
def tb(x,y,act=None,dark=False):
    t=['rect','ellipse','arrow','pen','text','mosaic','num']
    h=''.join(f'<div class="b {"on" if i==act else ""}" data-i="{i}"></div>' for i in t)
    h+='<div class="gap"></div><div class="b" data-i="undo"></div><div class="b dim" data-i="redo"></div><div class="gap"></div><div class="b" data-i="pin"></div><div class="b" data-i="save"></div><div class="b" data-i="close"></div><div class="b pri" data-i="ok"></div>'
    return f'<div class="tb glass {"dark" if dark else ""}" style="left:{x}px;top:{y}px">{h}</div>'
def sub(x,y,font=False,dark=False):
    cs=['#FF453A','#FF9F0A','#FFD60A','#32D74B','#4F6BFF','#BF5AF2','#1C1C1E','#FFFFFF']
    sw=''.join(f'<i style="width:18px;height:18px;border-radius:50%;background:{c};box-shadow:{"0 0 0 2px #fff,0 0 0 4px "+c if j==0 else "inset 0 0 0 1px rgba(0,0,0,.1)"}"></i>' for j,c in enumerate(cs))
    w=''.join(f'<div class="b {"on" if j==1 else ""}" style="width:30px;height:30px"><i style="width:{s}px;height:{s}px;border-radius:50%;background:currentColor"></i></div>' for j,s in enumerate((4,7,10)))
    f='<div class="gap"></div><span style="padding:0 8px;font-weight:500">16 <span style="opacity:.4">px</span></span><div class="b" style="width:30px;height:30px;font-weight:700">B</div>' if font else ''
    return f'<div class="tb glass {"dark" if dark else ""}" style="left:{x}px;top:{y}px;gap:10px;padding:6px 14px">{w if not font else ""}{"" if font else "<div class=gap></div>"}{sw}{f}</div>'
# 01a
page('01a-window-detect.html',S+'<div style="position:absolute;inset:0;background:rgba(15,18,30,.42);clip-path:polygon(0 0,100% 0,100% 100%,0 100%,0 150px,740px 150px,740px 600px,1340px 600px,1340px 150px,0 150px)"></div><div style="position:absolute;left:740px;top:150px;width:600px;height:450px;border-radius:12px;box-shadow:0 0 0 2px var(--a);background:rgba(79,107,255,.06)"></div>'+size(740,118,'季度报告.docx · 600 × 450')+mag(1040,410))
# 01b
X,Y,W,H=800,240,480,300
page('01b-region-select.html',S+sel(X,Y,W,H)+size(X,Y-32,f'{W} × {H}')+mag(X+W-160,Y+H+18))
# 02 light + dark variant
X,Y,W,H=780,180,520,330
page('02-annotation-toolbar.html',S+sel(X,Y,W,H)+size(X,Y-32,f'{W} × {H}')+tb(X+W-563,Y+H+14,'rect')+sub(X+W-563,Y+H+70))
page('02b-toolbar-dark.html',S+sel(X,Y,W,H)+size(X,Y-32,f'{W} × {H}')+tb(X+W-563,Y+H+14,'arrow',True)+sub(X+W-563,Y+H+70,dark=True))
# 03
X,Y,W,H=760,170,560,410
ann='''<svg style="position:absolute;left:0;top:0" width="1440" height="900"><defs><marker id="h" markerWidth="8" markerHeight="8" refX="5" refY="4" orient="auto"><path d="M0 0L8 4L0 8z" fill="#FF453A"/></marker></defs>
<rect x="1150" y="250" width="72" height="190" rx="10" fill="none" stroke="#FF453A" stroke-width="3"/>
<ellipse cx="880" cy="242" rx="110" ry="26" fill="none" stroke="#FF9F0A" stroke-width="3"/>
<path d="M1010 520 Q1080 500 1140 420" fill="none" stroke="#FF453A" stroke-width="3.5" stroke-linecap="round" marker-end="url(#h)"/>
<path d="M800 545 q20 -14 40 0 t40 0 t40 0" fill="none" stroke="#32D74B" stroke-width="3" stroke-linecap="round"/></svg>
<div style="position:absolute;left:880px;top:505px;color:#FF453A;font-size:17px;font-weight:600">峰值在 Q3 末</div>
<div style="position:absolute;left:772px;top:268px;width:260px;height:14px;border-radius:4px;background:repeating-conic-gradient(#d5d8e0 0 25%,#eceef3 0 50%) 0 0/7px 7px"></div>'''+''.join(f'<div style="position:absolute;left:{x}px;top:{y}px;width:22px;height:22px;border-radius:50%;background:#FF453A;color:#fff;font-size:12px;font-weight:600;display:flex;align-items:center;justify-content:center;box-shadow:0 2px 6px rgba(255,69,58,.4)">{i}</div>' for i,(x,y) in enumerate([(1210,238),(760,230),(858,503)],1))
page('03-annotated-example.html',S+sel(X,Y,W,H)+ann+tb(X+W-563,Y+H+14,'text')+sub(X+W-563,Y+H+70,font=True))
# 04
pinbody='<div style="font-size:18px;font-weight:600;margin:22px 24px 10px">第三季度回顾</div><div style="display:flex;gap:12px;margin:18px 24px;align-items:flex-end;height:150px"><div style="flex:1;height:55%;background:#c9d2ff;border-radius:7px"></div><div style="flex:1;height:75%;background:#9aabff;border-radius:7px"></div><div style="flex:1;height:45%;background:#c9d2ff;border-radius:7px"></div><div style="flex:1;height:100%;background:#4F6BFF;border-radius:7px"></div></div>'
mini=lambda ic:''.join(f'<div class="b" style="width:30px;height:30px" data-i="{i}"></div>' for i in ic)
page('04-pinned-window.html','<div class="wall"></div><div class="app" style="left:140px;top:90px;width:1160px;height:700px"><div style="height:44px"></div><div class="bar" style="width:30%"></div><div class="bar" style="width:50%"></div><div class="bar" style="width:40%"></div></div><div class="taskbar"><span class="logo"></span><span style="text-align:right;line-height:1.3">09:47<br>2026/10/21</span></div>'
 +f'<div style="position:absolute;left:520px;top:230px;width:420px;height:270px;background:#fff;border-radius:10px;box-shadow:0 0 0 1.5px var(--a),0 24px 60px rgba(20,24,40,.28)">{pinbody}<div class="tb glass" style="right:10px;top:10px;padding:3px;border-radius:10px;gap:0">{mini(["copy","save","close"])}</div><div class="pill" style="left:12px;bottom:12px">100%</div></div>'
 +f'<div style="position:absolute;left:220px;top:560px;width:240px;height:150px;background:#fff;border-radius:10px;opacity:.55;box-shadow:0 16px 40px rgba(20,24,40,.2);overflow:hidden"><div style="height:100%;background:linear-gradient(135deg,#a1c4fd,#5b7cff)"></div></div>')
# 05
item=lambda ic,t,k='',on=False:f'<div style="display:flex;align-items:center;gap:12px;padding:9px 12px;border-radius:8px;{"background:var(--at);color:var(--a)" if on else ""}"><span data-i="{ic}" style="display:flex;opacity:{1 if on else .7}"></span><span style="flex:1">{t}</span><span style="color:var(--t3);font-size:12px">{k}</span></div>'
hr='<div style="height:1px;background:var(--line);margin:5px 8px"></div>'
menu='<div class="glass" style="position:absolute;right:16px;bottom:60px;width:240px;border-radius:14px;padding:6px">'+item('crop','截图','F1',True)+item('screen','全屏截图','Ctrl F1')+item('pin','贴图','F3')+hr+item('gear','设置')+item('power','退出')+'</div>'
def row(l,c,d=''): return f'<div style="display:flex;align-items:center;padding:16px 20px;background:#fff;border-radius:10px;margin-bottom:8px;box-shadow:0 0 0 1px var(--line)"><div style="flex:1"><div style="font-weight:500">{l}</div>{f"<div style=color:var(--t3);font-size:12px;margin-top:2px>{d}</div>" if d else ""}</div>{c}</div>'
key=lambda k:f'<span style="padding:5px 10px;border-radius:7px;background:#f2f3f7;font-weight:500;font-size:12px">{k}</span>'
tog=lambda on:f'<span style="width:40px;height:22px;border-radius:11px;background:{"var(--a)" if on else "#d1d1d6"};position:relative;display:inline-block"><span style="position:absolute;top:2px;{"right" if on else "left"}:2px;width:18px;height:18px;border-radius:50%;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.25)"></span></span>'
seg='<span style="display:inline-flex;background:#f2f3f7;border-radius:8px;padding:2px;font-size:12px"><span style="padding:4px 14px;border-radius:6px;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.1);font-weight:500">PNG</span><span style="padding:4px 14px;color:var(--t2)">JPG</span></span>'
nav=''.join(f'<div style="display:flex;gap:10px;align-items:center;padding:9px 12px;border-radius:8px;margin-bottom:2px;{"background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.06);font-weight:500" if i==0 else "color:var(--t2)"}"><span data-i="{ic}" style="display:flex;{"color:var(--a)" if i==0 else ""}"></span>{t}</div>' for i,(ic,t) in enumerate([('gear','常规'),('crop','快捷键'),('info','关于')]))
st=f'''<div class="glass" style="position:absolute;left:300px;top:110px;width:680px;height:440px;border-radius:14px;overflow:hidden;background:rgba(246,247,251,.9)">
<div style="height:46px;display:flex;align-items:center;padding:0 18px;gap:10px;font-weight:500"><span class="logo"></span>设置<span style="margin-left:auto;opacity:.5" data-i="close"></span></div>
<div style="display:flex;height:474px"><div style="width:170px;padding:6px 10px">{nav}</div><div style="flex:1;padding:4px 24px 0 6px">
<div style="font-size:20px;font-weight:600;margin:6px 0 16px">常规</div>
{row('截图快捷键',key('F1'))}{row('保存位置','<span style="color:var(--t2);margin-right:12px">D:\\Pictures\\QingYing</span><span style="color:var(--a);font-weight:500">更改</span>')}{row('图片格式',seg)}{row('开机启动',tog(True),'登录后在托盘中运行')}{row('截图后自动保存',tog(False))}
</div></div></div>'''
page('05-tray-settings.html','<div class="wall"></div>'+st+menu+'<div class="taskbar"><span>中</span><span class="logo" style="box-shadow:0 0 0 5px rgba(79,107,255,.18)"></span><span style="text-align:right;line-height:1.3">09:47<br>2026/10/21</span></div>')
