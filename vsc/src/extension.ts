import * as vscode from "vscode";
import { readFileSync, existsSync } from "node:fs";
import { join } from "node:path";

const hoverCache: Record<string, string> = {};

export function activate(context: vscode.ExtensionContext) {
  const EXT_ROOT = context.extensionPath;

  vscode.languages.registerHoverProvider("clox", {
    provideHover(doc, position) {
      const wordRange = doc.getWordRangeAtPosition(position);

      if (!wordRange) {
        return;
      }

      const currentWord = doc.getText(wordRange);
      const cached = hoverCache[currentWord];

      if (cached) {
        return new vscode.Hover(new vscode.MarkdownString(cached));
      }

      const hoverFilePath = join(EXT_ROOT, "hovers", `${currentWord}.md`);

      if (!existsSync(hoverFilePath)) {
        return;
      }

      const hoverText = readFileSync(hoverFilePath, "utf8");
      hoverCache[currentWord] = hoverText;

      return new vscode.Hover(new vscode.MarkdownString(hoverText));
    },
  });

  const runCommand = vscode.commands.registerCommand("clox.run", () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) return;

    const file = editor.document.fileName;

    const terminal = vscode.window.createTerminal("Clox");
    terminal.show();

    terminal.sendText(`./build/clox -f "${file}"`);
  });

  context.subscriptions.push(runCommand);
}
