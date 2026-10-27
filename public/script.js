const list = document.getElementById('task-list');
const input = document.getElementById('task-input');
const form = document.getElementById('task-form');
const empty = document.getElementById('empty-state');
const message = document.getElementById('message');

document.getElementById('today').textContent = new Date().toLocaleDateString(undefined, {
  weekday: 'long', month: 'long', day: 'numeric'
});

async function api(path, data) {
  const options = data ? {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  } : {};
  const response = await fetch(path, options);
  if (!response.ok) throw new Error('Request failed');
  return response.json();
}

function showMessage(text = '') { message.textContent = text; }

function render(tasks) {
  list.replaceChildren();
  const done = tasks.filter(t => t.completed).length;
  document.getElementById('total-count').textContent = tasks.length;
  document.getElementById('left-count').textContent = tasks.length - done;
  document.getElementById('done-count').textContent = done;
  document.getElementById('progress-bar').style.width = tasks.length ? `${done / tasks.length * 100}%` : '0%';
  document.getElementById('progress-label').textContent = tasks.length
    ? `${done} of ${tasks.length} completed` : 'A fresh start ✨';
  empty.hidden = tasks.length > 0;

  tasks.forEach(task => {
    const li = document.createElement('li');
    li.className = `task${task.completed ? ' completed' : ''}`;
    const check = document.createElement('button');
    check.className = 'check';
    check.type = 'button';
    check.setAttribute('aria-label', task.completed ? 'Mark as not completed' : 'Mark as completed');
    check.textContent = task.completed ? '✓' : '';
    check.addEventListener('click', async () => {
      try { render((await api('/api/toggle', { id: task.id })).tasks); showMessage(); }
      catch { showMessage('Could not update task. Is the C server running?'); }
    });
    const text = document.createElement('span');
    text.className = 'task-text';
    text.textContent = task.text;
    const del = document.createElement('button');
    del.className = 'delete';
    del.type = 'button';
    del.setAttribute('aria-label', 'Delete task');
    del.textContent = '×';
    del.addEventListener('click', async () => {
      try { render((await api('/api/delete', { id: task.id })).tasks); showMessage(); }
      catch { showMessage('Could not delete task. Is the C server running?'); }
    });
    li.append(check, text, del);
    list.append(li);
  });
}

async function refresh() {
  try { render((await api('/api/tasks')).tasks); }
  catch { showMessage('Could not connect to the C server. Start todo_server.exe in VS Code.'); }
}

form.addEventListener('submit', async event => {
  event.preventDefault();
  const text = input.value.trim();
  if (!text) return;
  try {
    render((await api('/api/add', { text })).tasks);
    input.value = '';
    input.focus();
    showMessage();
  } catch { showMessage('Could not add task. Is the C server running?'); }
});

refresh();
