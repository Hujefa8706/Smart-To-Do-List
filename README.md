# Donezo To-Do List — C Backend + HTML/CSS/JavaScript

This project is designed for Windows with MinGW/GCC and VS Code. The C backend uses Winsock and a linked list. No WSL or Live Server extension is required.

## Files

- `server.c` — HTTP server, API, and linked-list task storage
- `public/index.html` — page structure
- `public/style.css` — styling
- `public/script.js` — frontend API calls

## Compile (VS Code PowerShell terminal)

Make sure the terminal is in the project folder (the folder containing `server.c`):

```powershell
gcc server.c -o todo_server.exe -lws2_32
```

The `-lws2_32` flag links Windows' Winsock networking library.

## Run

```powershell
.\todo_server.exe
```

You should see:

```text
Todo List server running at http://localhost:8080
Keep this terminal open. Press Ctrl+C to stop.
```

Open `http://localhost:8080` in your browser.

## DSA used

Tasks are stored in a singly linked list. Adding a task appends to the end, toggling searches by ID, and deleting removes a node from the list.

## Notes

- Keep the `public` folder beside `server.c`; run the executable from the project root.
- Tasks are stored in RAM and reset when the server stops.
- This is a learning/demo project, not intended for sensitive or production data.
- The page uses Google Fonts when internet is available; it still works offline with fallback fonts.
