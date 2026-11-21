import os
import re
import sys
import zipfile


def get_project_version(default='v1.2.4'):
    config_path = os.path.join('include', 'config.h')
    if not os.path.exists(config_path):
        return default
    with open(config_path, 'r', encoding='utf-8') as f:
        content = f.read()
    m = re.search(r'#define\s+PROJECT_VERSION\s+"([^"]+)"', content)
    return m.group(1) if m else default


def package():
    version = get_project_version()
    dist_dir = 'dist'
    os.makedirs(dist_dir, exist_ok=True)
    zip_path = os.path.join(dist_dir, f'Bailan_MP3_{version}_WebFlasher.zip')
    if os.path.exists(zip_path):
        os.remove(zip_path)

    files_to_pack = [
        ('flash_web.bat', 'flash_web.bat'),
        ('本地网页刷机.bat', '本地网页刷机.bat'),
    ]

    for root, dirs, files in os.walk('web_flasher'):
        if '__pycache__' in root:
            continue
        for file in files:
            if file.endswith('.pyc'):
                continue
            full_path = os.path.join(root, file)
            arcname = os.path.relpath(full_path, '.')
            files_to_pack.append((full_path, arcname))

    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        for full_path, arcname in files_to_pack:
            norm_arc = arcname.replace('\\', '/')
            zf.write(full_path, norm_arc)

    print(f"Successfully packaged: {zip_path}")
    print(f"File size: {os.path.getsize(zip_path):,} bytes")
    with zipfile.ZipFile(zip_path, 'r') as zf:
        for info in zf.infolist():
            print(f"  [{hex(info.flag_bits)}] {info.filename}")

if __name__ == '__main__':
    package()
