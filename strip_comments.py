import os
import re

def strip_comments(text):
    # Regex to match strings or block comments or line comments
    # We will preserve strings and block comments, and remove line comments
    pattern = r'(".*?"|\'.*?\'|/\*.*?\*/)|(//.*)'
    
    def replacer(match):
        if match.group(2) is not None:
            # It's a line comment, replace with empty string if it's the only thing on the line,
            # or preserve the newline if we don't want to break things. But actually returning nothing removes it.
            return ""
        else:
            # It's a string or block comment, preserve it
            return match.group(1)

    # We also want to remove empty lines that were left behind
    lines = text.split('\n')
    new_lines = []
    for line in lines:
        stripped = re.sub(pattern, replacer, line)
        if stripped.strip() or not line.strip(): # keep original empty lines, but drop lines that became empty
            # If line wasn't empty but became empty, skip it. Unless it only had spaces.
            if line.strip() and not stripped.strip():
                continue
            # Also clean up trailing whitespace
            new_lines.append(stripped.rstrip())
        else:
            new_lines.append(stripped)
    
    return '\n'.join(new_lines)


def process_dir(directory):
    for root, _, files in os.walk(directory):
        if 'build' in root or 'dist' in root or '.git' in root:
            continue
        for file in files:
            if file.endswith(('.cpp', '.h')):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                new_content = strip_comments(content)
                
                if new_content != content:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    print(f"Cleaned {filepath}")

if __name__ == "__main__":
    process_dir('.')
