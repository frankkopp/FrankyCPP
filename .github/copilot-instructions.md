## Additional Windows \+ multi\-language Copilot instructions

### General \& PowerShell
- Assume Windows PowerShell. Use `powershell` code fences for shell commands.
- Do not chain commands with `\&\&`, `;`, or Unix\-only syntax. Put each command in its own code block.
- Use `.\` to run local executables.
- Do not try to compile code as part of your response as you do not have the Clion environment. Ask the user to compile and run the code on their end and ask for any errors or issues they encounter.
- Use `$env:VAR = "value"` to set env vars for the current session.
- For persistent env vars, use:
```powershell
[System.Environment]::SetEnvironmentVariable('VAR','value','User')
``` 
