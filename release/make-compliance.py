# Run with Python 3 from a configured Windows developer environment.
from pathlib import Path
import json,re,shutil,subprocess
root=Path(chr(92)*2+'?'+chr(92)+str(Path(__file__).resolve().parents[1])); sdk=root/'tools/rexglue-sdk'; build=sdk/'out/build/win-amd64'
out=root/'out/releases/v1.0.0-rc2-LGPL-Compliance'
if out.exists(): raise SystemExit('Refusing to overwrite compliance staging')
out.mkdir(parents=True)
cmd=json.loads(subprocess.check_output(['ninja','-C',str(build),'-f','build-Release.ninja','-t','compdb','-x','CXX_SHARED_LIBRARY_LINKER__rexruntime_Release']))[0]['command']
cmd=cmd[cmd.index('-nostartfiles'):].split('  && cd .')[0]
tokens=[x.replace('"','') for x in re.findall(r'(?:[^\s"]+|"[^"]*")+',cmd)]
args=[]; i=0; count=0
while i<len(tokens):
 t=tokens[i]
 if t=='-o': args+=['-o','rexruntime.dll'];i+=2;continue
 if t.startswith('/implib:'): args+=['/implib:rexruntime.lib'];i+=1;continue
 if t.startswith('/pdb:'): args+=['/pdb:rexruntime.pdb'];i+=1;continue
 if t.startswith('/DEF:'):
  shutil.copy2(build/Path(t[5:]),out/'exports.def');args+=['/DEF:exports.def'];i+=1;continue
 if t.endswith(('.obj','.lib')) and not t.startswith('-l'):
  src=Path(t);src=src if src.is_absolute() else build/src
  rel=Path('libraries')/src.name if src.suffix=='.lib' else Path('objects')/src.relative_to(build)
  dst=out/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst);args.append(rel.as_posix());count+=1
 else: args.append(t)
 i+=1
(out/'relink.rsp').write_text('\n'.join('"'+t+'"' for t in args)+'\n')
files=subprocess.check_output(['git','-C',str(sdk),'ls-files','--recurse-submodules','-z']).decode().split('\0')
for name in filter(None,files):
 src=sdk/name
 if not src.is_file(): raise RuntimeError(f'Missing SDK source: {name}')
 dst=out/'sdk-source'/name;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
shutil.copy2(root/'tools/rexglue-install/local-build.json',out/'SDK_BUILD.json')
shutil.copy2(root/'tools/rexglue-install/local-source.diff',out/'SDK_CHANGES.patch')
shutil.copytree(root/'LICENSES',out/'LICENSES')
print(f'Compliance staged: {len(files)-1} SDK source files, {count} link inputs')
print(f'Total size: {sum(p.stat().st_size for p in out.rglob("*") if p.is_file())/1024**2:.1f} MiB')



# A source archive has no Git metadata; validate the vendored directories.
p=out/'sdk-source/thirdparty/CMakeLists.txt'
t=p.read_text().replace('if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git")','if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}")')
p.write_text(t)
for p in (root/'release/compliance').iterdir():
 shutil.copy2(p,out/p.name)
