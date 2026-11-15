# Packs the .arduboy files (zip with info.json) used by the FX cart builders,
# the web uploader and emulators like Ardens / ProjectABE.
import json, os, sys, zipfile
out = sys.argv[1]
here = os.path.dirname(os.path.abspath(__file__))
cart = os.path.join(here, '..', 'ArduwarsFX', 'cartimage.png')
for hexname, device, label in [('ArduwarsFX-FXC.hex', 'ArduboyMini', 'Arduboy FX-C / Mini (flash select on HWB)'),
                               ('ArduwarsFX-FX.hex',  'ArduboyFX',   'Arduboy FX (flash select on SDA)')]:
    info = {
        'schemaVersion': 3,
        'title': 'Arduwars FX',
        'version': '1.1',
        'author': 'Botond Kis (original), FX port 2026',
        'description': 'Advance Wars style turn based strategy. Vs computer or 2 players, 3 maps, fog of war, 20 units a side, battle scenes, autosave.',
        'genre': 'Strategy',
        'license': 'BSD-3-Clause',
        'url': 'https://github.com/BotiKis/Arduwars',
        'binaries': [{
            'title': label,
            'filename': hexname,
            'device': device,
            'flashdata': 'fxdata-data.bin',
            'flashsave': 'fxdata-save.bin',
            'cartimage': 'cartimage.png'
        }]
    }
    name = os.path.join(out, hexname.replace('.hex', '.arduboy'))
    with zipfile.ZipFile(name, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('info.json', json.dumps(info, indent=2))
        z.write(os.path.join(out, hexname), hexname)
        z.write(os.path.join(out, 'fxdata-data.bin'), 'fxdata-data.bin')
        z.write(os.path.join(out, 'fxdata-save.bin'), 'fxdata-save.bin')
        z.write(cart, 'cartimage.png')
    print('wrote', name)
