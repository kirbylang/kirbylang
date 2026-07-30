import * as vscode from "vscode";
import { readFileSync, existsSync } from "node:fs";
import { isAbsolute, join } from "node:path";

const hoverCache = new Map<string, string | null>();

function shellQuote(value: string) {
  return `"${value.replace(/(["$`\\])/g, "\\$1")}"`;
}

export function activate(context: vscode.ExtensionContext) {
  const EXT_ROOT = context.extensionPath;

  const hoverProvider = vscode.languages.registerHoverProvider("clox", {
    provideHover(doc, position) {
      const wordRange = doc.getWordRangeAtPosition(position);

      if (!wordRange) {
        return;
      }

      const currentWord = doc.getText(wordRange);

      if (hoverCache.has(currentWord)) {
        const cached = hoverCache.get(currentWord);

        return cached
          ? new vscode.Hover(new vscode.MarkdownString(cached))
          : undefined;
      }

      const hoverFilePath = join(EXT_ROOT, "hovers", `${currentWord}.md`);

      if (!existsSync(hoverFilePath)) {
        hoverCache.set(currentWord, null);

        return;
      }

      const hoverText = readFileSync(hoverFilePath, "utf8");
      hoverCache.set(currentWord, hoverText);

      return new vscode.Hover(new vscode.MarkdownString(hoverText));
    },
  });

  const runCommand = vscode.commands.registerCommand("clox.run", () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) return;

    const document = editor.document;
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);

    const configured = vscode.workspace
      .getConfiguration("clox")
      .get<string>("executablePath", "./build/clox");

    const executable =
      isAbsolute(configured) || !workspaceFolder
        ? configured
        : join(workspaceFolder.uri.fsPath, configured);

    const terminalOptions: vscode.TerminalOptions = { name: "Clox" };

    if (workspaceFolder) {
      terminalOptions.cwd = workspaceFolder.uri;
    }

    const terminal = vscode.window.createTerminal(terminalOptions);
    terminal.show();

    terminal.sendText(
      `${shellQuote(executable)} -f ${shellQuote(document.uri.fsPath)}`,
    );
  });

  context.subscriptions.push(hoverProvider, runCommand);
}
