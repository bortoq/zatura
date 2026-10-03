#!/usr/bin/env python3
"""Generate a complete, annotated zaturarc from the actual build and sources."""
import ast, json, pathlib, re, shlex, sys
root=pathlib.Path(__file__).resolve().parents[1]
settings=json.loads(pathlib.Path(sys.argv[1]).read_text())
source=(root/'zatura/config.c').read_text()
def args(text):
    return [x.strip() for x in re.split(r',(?=(?:[^"\\]*(?:\\.[^"\\]*)*"[^"\\]*(?:\\.[^"\\]*)*")*[^"\\]*(?:\\.[^"\\]*)*$)',text)]
def calls(name):
    return [args(m.group(1)) for m in re.finditer(r'\b'+name+r'\((.*?)\);',source,re.S)]
def string(value): return ast.literal_eval(value)
def quote(value): return '"'+str(value).replace('\\','\\\\').replace('"','\\"').replace('\n','\\n')+'"'
lines=['# Generated defaults. Copy only overrides into ~/.config/zatura/zaturarc.',
       '# Options: doc/man/zaturarc.5.rst. C=Ctrl, A=Alt, S=Shift; physical US keys.', '']
for name,s in settings.items():
    value=s['value']
    formatted=('true' if value else 'false') if isinstance(value,bool) else quote(value) if isinstance(value,str) else str(value)
    lines += ['set '+name+' '+formatted+(' # startup only' if s['init-only'] else '')]
lines += ['']
# Source constants map onto public config identifiers, avoiding enum collisions.
function_map={a[2]:string(a[1]) for a in calls('girara_shortcut_mapping_add') if len(a)==3}
arg_map={a[2]:string(a[1]) for a in calls('girara_argument_mapping_add') if len(a)==3}
arg_map.update({'-1':'down','1':'up'})
key_names={m.group(2):m.group(1) for m in re.finditer(r'\{"([^"\n]+)",\s*GDK_KEY_(\w+)\}',(root/'girara-gtk/commands.c').read_text())}
chars={'plus':'+','minus':'-','equal':'=','slash':'/','question':'?','colon':':','apostrophe':"'",'bracketleft':'['}
def key(mask,code,buffer='NULL'):
    if buffer!='NULL': return string(buffer)
    name=code.removeprefix('GDK_KEY_')
    name=chars.get(name,key_names.get(name,name))
    modifiers=''.join(prefix+'-' for c,prefix in [('GDK_CONTROL_MASK','C'),('GDK_ALT_MASK','A'),('GDK_SHIFT_MASK','S')] if c in mask)
    # Explicit Shift on punctuation duplicates the key's implicit Shift.
    if name in ['+','?',':'] or (len(name)==1 and name.isupper()): modifiers=modifiers.replace('S-','')
    return '<'+modifiers+name+'>' if modifiers or len(name)>1 else name
modes={'NORMAL':['normal'],'INSERT':['insert'],'INDEX':['index'],'PRESENTATION':['presentation'],'mode':['normal'],'all_modes[idx]':['normal','insert','index','presentation']}
records=[]
for a in calls('girara_shortcut_add'):
    if len(a)!=8 or a[4]=='sc_adjust_page_effect': continue
    _,mask,code,buffer,func,mode,num,data=a
    if mode not in modes: raise ValueError(mode)
    name=function_map.get(func)
    if not name: raise ValueError('No mapping '+func)
    argument=arg_map.get(num, '') if num!='0' else ''
    if num!='0' and not argument: raise ValueError('No argument '+num)
    extra=((' '+argument) if argument else '')+((' '+quote(string(data))) if data!='NULL' else '')
    for mode_name in modes[mode]: records.append((mode_name,key(mask,code,buffer),name,extra))
for a in calls('girara_inputbar_shortcut_add'):
    _,mask,code,func,num,data=a
    name=function_map[func]; argument=arg_map.get(num,'') if num!='0' else ''
    if num!='0' and not argument: raise ValueError(num)
    records.append(('inputbar',key(mask,code),name, (' '+argument if argument else '')+(' '+quote(string(data)) if data!='NULL' else '')))
for mode in ['normal','presentation']:
    for i,func in enumerate(['adjust_contrast','adjust_brightness','adjust_gamma','adjust_saturation']):
        records += [(mode,str(1+2*i),func,' down'), (mode,str(2+2*i),func,' up')]
unique={record[:2]:record for record in records}
for mode in ['normal','presentation','index','insert','inputbar']:
    lines += ['', '# '+mode]
    for m,binding,func,extra in unique.values():
        if m==mode: lines += ['map ['+mode+'] '+shlex.quote(binding)+' '+func+extra]
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
print(f'{len(settings)} settings; {len(unique)} keyboard bindings')
