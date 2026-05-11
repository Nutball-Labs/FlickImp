'use strict';

// ---------- API helpers ---------------------------------------------------

async function api(method, path, body) {
    const opts = { method, headers: { 'Content-Type': 'application/json' } };
    if (body !== undefined) opts.body = JSON.stringify(body);
    const res = await fetch(path, opts);
    return res.json();
}

// ---------- Utility -------------------------------------------------------

function esc(str) {
    return String(str ?? '')
        .replace(/&/g,'&amp;').replace(/</g,'&lt;')
        .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function ep(season, episode) {
    return `S${String(season).padStart(2,'0')}E${String(episode).padStart(2,'0')}`;
}

// ---------- Shows ---------------------------------------------------------

async function loadShows() {
    const shows = await api('GET', '/api/shows');
    renderShows(shows);
}

function showStatusBadge(status) {
    const map = {
        flickimp: ['badge-watching', 'FlickImp'],
        paused:   ['badge-paused',   'Paused'],
        finished: ['badge-finished', 'Finished'],
    };
    const [cls, label] = map[status] ?? ['badge-watching', status];
    return `<span class="badge ${cls}">${label}</span>`;
}

function showCard(s) {
    const epLabel = ep(s.season, s.episode);
    const total = s.total_episodes > 0
        ? `<span class="ep-total">/ ${s.total_episodes}</span>`
        : '';
    const notesHtml = s.notes
        ? `<div class="card-notes">${esc(s.notes)}</div>`
        : '';

    const pauseLabel = s.status === 'paused' ? '▶ Resume' : '⏸ Pause';

    return `
    <div class="card" data-id="${s.id}">
      <div class="card-top">
        <div class="card-title">${esc(s.title)}</div>
        ${showStatusBadge(s.status)}
      </div>
      <div class="card-meta">
        ${s.service ? `<span class="service-tag">${esc(s.service)}</span>` : ''}
        <span class="ep-display">${epLabel}</span>${total}
      </div>
      ${notesHtml}
      <div class="card-actions">
        <button class="btn-ep" title="Previous episode"
                onclick="changeEpisode(${s.id},-1)">◀</button>
        <button class="btn-ep" title="Next episode"
                onclick="changeEpisode(${s.id},+1)">▶</button>
        <button class="btn-action" onclick="cycleStatus(${s.id},'${s.status}')">
          ${pauseLabel}
        </button>
        <button class="btn-danger" onclick="deleteShow(${s.id})">Remove</button>
      </div>
    </div>`;
}

function renderShows(shows) {
    const list = document.getElementById('shows-list');
    if (!shows.length) {
        list.innerHTML = '<div class="empty">No shows yet — add one above.</div>';
        return;
    }
    list.innerHTML = shows.map(showCard).join('');
}

async function changeEpisode(id, delta) {
    const shows = await api('GET', '/api/shows');
    const s = shows.find(x => x.id === id);
    if (!s) return;
    let season = s.season, episode = s.episode + delta;
    if (episode < 1) { season = Math.max(1, season - 1); episode = 1; }
    await api('PUT', `/api/shows/${id}`, { season, episode });
    loadShows();
}

async function cycleStatus(id, current) {
    const next = current === 'paused' ? 'watching' : 'paused';
    await api('PUT', `/api/shows/${id}`, { status: next });
    loadShows();
}

async function deleteShow(id) {
    if (!confirm('Remove this show?')) return;
    await api('DELETE', `/api/shows/${id}`);
    loadShows();
}

function showAddShowForm()  {
    document.getElementById('add-show-form').classList.remove('hidden');
    document.getElementById('new-show-title').focus();
}
function hideAddShowForm()  {
    document.getElementById('add-show-form').classList.add('hidden');
    clearShowForm();
}
function clearShowForm() {
    ['new-show-title','new-show-service','new-show-imdb']
        .forEach(id => { document.getElementById(id).value = ''; });
    document.getElementById('new-show-season').value  = '1';
    document.getElementById('new-show-episode').value = '1';
}

async function addShow() {
    const title = document.getElementById('new-show-title').value.trim();
    if (!title) { alert('Title is required.'); return; }
    await api('POST', '/api/shows', {
        title,
        service: document.getElementById('new-show-service').value.trim(),
        season:  parseInt(document.getElementById('new-show-season').value)  || 1,
        episode: parseInt(document.getElementById('new-show-episode').value) || 1,
        imdb_id: document.getElementById('new-show-imdb').value.trim(),
    });
    hideAddShowForm();
    loadShows();
}

// ---------- Movies --------------------------------------------------------

async function loadMovies() {
    const movies = await api('GET', '/api/movies');
    renderMovies(movies);
}

function movieStatusBadge(status) {
    const map = {
        want_to_watch: ['badge-want',    'Want to watch'],
        watched:       ['badge-watched', 'Watched'],
    };
    const [cls, label] = map[status] ?? ['badge-want', status];
    return `<span class="badge ${cls}">${label}</span>`;
}

function movieCard(m) {
    const notesHtml = m.notes
        ? `<div class="card-notes">${esc(m.notes)}</div>`
        : '';
    const watchedLabel = m.status === 'watched' ? '↩ Unwatch' : '✓ Watched';

    return `
    <div class="card" data-id="${m.id}">
      <div class="card-top">
        <div class="card-title">${esc(m.title)}</div>
        ${movieStatusBadge(m.status)}
      </div>
      ${notesHtml}
      <div class="card-actions">
        <button class="btn-watched" onclick="toggleWatched(${m.id},'${m.status}')">
          ${watchedLabel}
        </button>
        <button class="btn-danger" onclick="deleteMovie(${m.id})">Remove</button>
      </div>
    </div>`;
}

function renderMovies(movies) {
    const list = document.getElementById('movies-list');
    if (!movies.length) {
        list.innerHTML = '<div class="empty">No movies yet — add one above.</div>';
        return;
    }
    list.innerHTML = movies.map(movieCard).join('');
}

async function toggleWatched(id, current) {
    const next = current === 'watched' ? 'want_to_watch' : 'watched';
    await api('PUT', `/api/movies/${id}`, { status: next });
    loadMovies();
}

async function deleteMovie(id) {
    if (!confirm('Remove this movie?')) return;
    await api('DELETE', `/api/movies/${id}`);
    loadMovies();
}

function showAddMovieForm() {
    document.getElementById('add-movie-form').classList.remove('hidden');
    document.getElementById('new-movie-title').focus();
}
function hideAddMovieForm() {
    document.getElementById('add-movie-form').classList.add('hidden');
    clearMovieForm();
}
function clearMovieForm() {
    ['new-movie-title','new-movie-imdb','new-movie-notes']
        .forEach(id => { document.getElementById(id).value = ''; });
}

async function addMovie() {
    const title = document.getElementById('new-movie-title').value.trim();
    if (!title) { alert('Title is required.'); return; }
    await api('POST', '/api/movies', {
        title,
        imdb_id: document.getElementById('new-movie-imdb').value.trim(),
        notes:   document.getElementById('new-movie-notes').value.trim(),
    });
    hideAddMovieForm();
    loadMovies();
}

// ---------- Init ----------------------------------------------------------

loadShows();
loadMovies();
