
import sys
import os
import json
import urllib.parse
import urllib.request
import urllib.error
import subprocess
import re
from html.parser import HTMLParser

class DDGHTMLParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.results = []
        self.in_result = False
        self.in_title = False
        self.in_snippet = False
        self.current_result = {}
        self.temp_snippet = []

    def handle_starttag(self, tag, attrs):
        attrs_dict = dict(attrs)
        
        if tag == 'div' and 'class' in attrs_dict and 'web-result' in attrs_dict['class']:
            self.in_result = True
            self.current_result = {'title': '', 'link': '', 'snippet': ''}
            self.temp_snippet = []
        
        
        elif self.in_result and tag == 'a' and 'class' in attrs_dict and 'result__url' in attrs_dict['class']:
            self.in_title = True
            if 'href' in attrs_dict:
                
                link = attrs_dict['href']
                if link.startswith('//duckduckgo.com/l/?uddg='):
                    parsed = urllib.parse.urlparse(link)
                    qs = urllib.parse.parse_qs(parsed.query)
                    if 'uddg' in qs:
                        link = qs['uddg'][0]
                self.current_result['link'] = link

        
        elif self.in_result and tag == 'a' and 'class' in attrs_dict and 'result__snippet' in attrs_dict['class']:
            self.in_snippet = True

    def handle_endtag(self, tag):
        if tag == 'div' and self.in_result:
            
            if self.current_result.get('link'):
                self.current_result['snippet'] = " ".join(self.temp_snippet).strip()
                self.results.append(self.current_result)
            self.in_result = False
            self.current_result = {}
            self.temp_snippet = []
        elif tag == 'a':
            self.in_title = False
            self.in_snippet = False

    def handle_data(self, data):
        if self.in_title:
            self.current_result['title'] = (self.current_result.get('title', '') + data).strip()
        elif self.in_snippet:
            self.temp_snippet.append(data.strip())

def web_search(query):
    try:
        url = f"https://html.duckduckgo.com/html/?q={urllib.parse.quote(query)}"
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36'}
        )
        with urllib.request.urlopen(req, timeout=10) as response:
            html = response.read().decode('utf-8')
        
        parser = DDGHTMLParser()
        parser.feed(html)
        
        
        
        results = parser.results[:8]
        return json.dumps({"status": "success", "results": results}, indent=2)
    except Exception as e:
        return json.dumps({"status": "error", "message": str(e)})

def capture_screenshot(output_path):
    
    try:
        import dbus
        bus = dbus.SessionBus()
        screenshot_obj = bus.get_object('org.gnome.Shell.Screenshot', '/org/gnome/Shell/Screenshot')
        screenshot_iface = dbus.Interface(screenshot_obj, 'org.gnome.Shell.Screenshot')
        
        
        abs_path = os.path.abspath(output_path)
        
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        success, filename = screenshot_iface.Screenshot(True, False, abs_path)
        if success:
            return json.dumps({"status": "success", "path": filename})
    except Exception as e:
        
        pass
    
    try:
        abs_path = os.path.abspath(output_path)
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        subprocess.run(["gnome-screenshot", "-f", abs_path], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return json.dumps({"status": "success", "path": abs_path})
    except Exception as e:
        return json.dumps({"status": "error", "message": f"GNOME DBus screenshot failed and gnome-screenshot CLI failed: {e}"})

def control_gui(action, args):
    
    
    try:
        if action == "click":
            x, y = args.get("x"), args.get("y")
            if x is not None and y is not None:
                subprocess.run(["xdotool", "mousemove", str(x), str(y), "click", "1"], check=True)
                return json.dumps({"status": "success"})
        elif action == "move":
            x, y = args.get("x"), args.get("y")
            if x is not None and y is not None:
                subprocess.run(["xdotool", "mousemove", str(x), str(y)], check=True)
                return json.dumps({"status": "success"})
        elif action == "type":
            text = args.get("text")
            if text:
                
                subprocess.run(["xdotool", "type", "--delay", "10", text], check=True)
                return json.dumps({"status": "success"})
        elif action == "key":
            key = args.get("key")
            if key:
                subprocess.run(["xdotool", "key", key], check=True)
                return json.dumps({"status": "success"})
        return json.dumps({"status": "error", "message": f"Invalid action '{action}' or missing arguments."})
    except Exception as e:
        return json.dumps({"status": "error", "message": f"xdotool execution failed: {e}. Note: GUI automation is limited on native Wayland windows; try focusing an XWayland window."})

def generate_image(prompt, output_path):
    try:
        
        
        abs_path = os.path.abspath(output_path)
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        
        encoded_prompt = urllib.parse.quote(prompt)
        url = f"https://image.pollinations.ai/prompt/{encoded_prompt}?width=1024&height=1024&nologo=true"
        
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'}
        )
        with urllib.request.urlopen(req, timeout=30) as response:
            data = response.read()
            
        with open(abs_path, 'wb') as f:
            f.write(data)
            
        return json.dumps({"status": "success", "path": abs_path})
    except urllib.error.HTTPError as e:
        if e.code == 402:
            return json.dumps({"status": "error", "message": "Pollinations.ai rate limit/queue is full for this IP (HTTP 402). Try again shortly or register at https://enter.pollinations.ai"})
        return json.dumps({"status": "error", "message": f"HTTP Error {e.code}: {e.reason}"})
    except Exception as e:
        return json.dumps({"status": "error", "message": str(e)})

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"status": "error", "message": "No command provided."}))
        sys.exit(1)
        
    command = sys.argv[1]
    
    if command == "search":
        if len(sys.argv) < 3:
            print(json.dumps({"status": "error", "message": "Search query missing."}))
            sys.exit(1)
        print(web_search(sys.argv[2]))
        
    elif command == "screenshot":
        if len(sys.argv) < 3:
            print(json.dumps({"status": "error", "message": "Output path missing."}))
            sys.exit(1)
        print(capture_screenshot(sys.argv[2]))
        
    elif command == "gui":
        if len(sys.argv) < 4:
            print(json.dumps({"status": "error", "message": "GUI action or args missing."}))
            sys.exit(1)
        action = sys.argv[2]
        try:
            args = json.loads(sys.argv[3])
        except Exception as e:
            print(json.dumps({"status": "error", "message": f"Invalid JSON args: {e}"}))
            sys.exit(1)
        print(control_gui(action, args))
        
    elif command == "image":
        if len(sys.argv) < 4:
            print(json.dumps({"status": "error", "message": "Prompt or output path missing."}))
            sys.exit(1)
        print(generate_image(sys.argv[2], sys.argv[3]))
        
    else:
        print(json.dumps({"status": "error", "message": f"Unknown command: {command}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()
