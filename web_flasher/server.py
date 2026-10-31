import http.server
import socketserver
import webbrowser
import os
import sys

PORT = 8000
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def log_message(self, format, *args):
        # 简化日志输出，避免无谓刷屏
        if "GET" in args[0] and "200" not in args[1]:
            super().log_message(format, *args)

def run():
    os.chdir(DIRECTORY)
    for p in range(PORT, PORT + 20):
        try:
            with socketserver.TCPServer(("", p), Handler) as httpd:
                url = f"http://localhost:{p}/"
                print("=" * 60)
                print(f"  CYD Plus 网页一键刷机服务已启动!")
                print(f"  访问地址: {url}")
                print("=" * 60)
                print("正在为您打开浏览器...")
                webbrowser.open(url)
                print("\n提示: 刷机完成后，在本窗口按 Ctrl+C 即可退出服务。")
                try:
                    httpd.serve_forever()
                except KeyboardInterrupt:
                    print("\n服务已安全退出。")
                    sys.exit(0)
                break
        except OSError:
            continue

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--check':
        print("CYD Plus Web Flasher server check OK")
        sys.exit(0)
    run()
