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

// Format season/episode as s000-e000
function ep(season, episode) {
    return `s${String(season).padStart(3,'0')}-e${String(episode).padStart(3,'0')}`;
}

// Parse s000-e000 → { season, episode } or null on bad input
function parseEp(str) {
    const m = (str || '').trim().toLowerCase().match(/^s(\d+)-e(\d+)$/);
    if (!m) return null;
    return { season: parseInt(m[1], 10), episode: parseInt(m[2], 10) };
}

function pad3(n) { return String(n).padStart(3, '0'); }

// ---------- TMDB search state --------------------------------------------

let g_new_show_tmdb_id  = 0;
let g_new_show_thumb    = '';
let g_new_movie_tmdb_id = 0;
let g_new_movie_thumb   = '';

// ---------- TMDB search --------------------------------------------------

function searchResultHtml(r, selectFn) {
    const img = r.poster_url
        ? `<img class="search-result-thumb" src="${esc(r.poster_url)}" alt="" loading="lazy">`
        : `<div class="search-result-thumb-placeholder"></div>`;
    const year = r.year ? `<div class="search-result-year">${esc(r.year)}</div>` : '';
    const titleArg = JSON.stringify(r.title).replace(/"/g, '&quot;');
    const thumbArg = JSON.stringify(r.poster_url || '').replace(/"/g, '&quot;');
    return `
    <div class="search-result-item"
         onclick="${selectFn}(${r.tmdb_id}, ${titleArg}, ${thumbArg})">
      ${img}
      <div class="search-result-info">
        <div class="search-result-title">${esc(r.title)}</div>
        ${year}
      </div>
    </div>`;
}

async function searchShows() {
    const q = document.getElementById('new-show-title').value.trim();
    if (!q) return;
    const pane = document.getElementById('show-search-results');
    pane.innerHTML = '<div class="search-msg">Searching…</div>';
    pane.classList.remove('hidden');
    try {
        const results = await api('GET', `/api/search/shows?q=${encodeURIComponent(q)}`);
        if (results.error) {
            pane.innerHTML = `<div class="search-msg">${esc(results.error)}</div>`;
        } else if (!Array.isArray(results) || !results.length) {
            pane.innerHTML = '<div class="search-msg">No results.</div>';
        } else {
            pane.innerHTML = results.map(r => searchResultHtml(r, 'selectShowResult')).join('');
        }
    } catch {
        pane.innerHTML = '<div class="search-msg">Search failed.</div>';
    }
}

async function searchMovies() {
    const q = document.getElementById('new-movie-title').value.trim();
    if (!q) return;
    const pane = document.getElementById('movie-search-results');
    pane.innerHTML = '<div class="search-msg">Searching…</div>';
    pane.classList.remove('hidden');
    try {
        const results = await api('GET', `/api/search/movies?q=${encodeURIComponent(q)}`);
        if (results.error) {
            pane.innerHTML = `<div class="search-msg">${esc(results.error)}</div>`;
        } else if (!Array.isArray(results) || !results.length) {
            pane.innerHTML = '<div class="search-msg">No results.</div>';
        } else {
            pane.innerHTML = results.map(r => searchResultHtml(r, 'selectMovieResult')).join('');
        }
    } catch {
        pane.innerHTML = '<div class="search-msg">Search failed.</div>';
    }
}

function selectShowResult(tmdbId, title, posterUrl) {
    g_new_show_tmdb_id = tmdbId;
    g_new_show_thumb   = posterUrl;
    document.getElementById('new-show-title').value = title;
    document.getElementById('show-search-results').classList.add('hidden');
}

function selectMovieResult(tmdbId, title, posterUrl) {
    g_new_movie_tmdb_id = tmdbId;
    g_new_movie_thumb   = posterUrl;
    document.getElementById('new-movie-title').value = title;
    document.getElementById('movie-search-results').classList.add('hidden');
}

// ---------- Drag-and-drop reorder ----------------------------------------

let g_drag_id = 0;
let g_drag_type = '';  // 'show' or 'movie'
let g_can_drag = false;

document.addEventListener('mouseup', () => { g_can_drag = false; });

function dragHandleDown(event) {
    g_can_drag = true;
    event.stopPropagation();
}

function itemDragStart(event, type, id) {
    if (!g_can_drag) { event.preventDefault(); return; }
    g_drag_id = id;
    g_drag_type = type;
    event.dataTransfer.effectAllowed = 'move';
    event.dataTransfer.setData('text/plain', String(id));
    const wrap = event.currentTarget;
    setTimeout(() => wrap.classList.add('dragging'), 0);
}

function itemDragEnd(event) {
    event.currentTarget.classList.remove('dragging');
    document.querySelectorAll('.drag-over-before, .drag-over-after')
        .forEach(el => { el.classList.remove('drag-over-before', 'drag-over-after'); });
    g_drag_id = 0;
    g_drag_type = '';
}

function itemDragOver(event) {
    if (!g_drag_id) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = 'move';
    const wrap = event.currentTarget;
    if (wrap.classList.contains('dragging')) return;
    document.querySelectorAll('.drag-over-before, .drag-over-after')
        .forEach(el => { el.classList.remove('drag-over-before', 'drag-over-after'); });
    const rect = wrap.getBoundingClientRect();
    const midX = rect.left + rect.width / 2;
    if (event.clientX < midX) {
        wrap.classList.add('drag-over-before');
        const prev = wrap.previousElementSibling;
        if (prev && !prev.classList.contains('dragging')) {
            const prevRect = prev.getBoundingClientRect();
            if (Math.abs(prevRect.top - rect.top) > 10)
                prev.classList.add('drag-over-after');
        }
    } else {
        wrap.classList.add('drag-over-after');
        const next = wrap.nextElementSibling;
        if (next && !next.classList.contains('dragging')) {
            const nextRect = next.getBoundingClientRect();
            if (Math.abs(nextRect.top - rect.top) > 10)
                next.classList.add('drag-over-before');
        }
    }
}

function itemDragLeave(event) {
    const wrap = event.currentTarget;
    if (!wrap.contains(event.relatedTarget))
        wrap.classList.remove('drag-over-before', 'drag-over-after');
}

function showDrop(event, targetId) {
    event.preventDefault();
    const targetWrap = event.currentTarget;
    const insertAfter = targetWrap.classList.contains('drag-over-after');
    document.querySelectorAll('.drag-over-before, .drag-over-after')
        .forEach(el => { el.classList.remove('drag-over-before', 'drag-over-after'); });
    if (!g_drag_id || g_drag_id === targetId || g_drag_type !== 'show') return;

    const isQueued = g_shows_tab === 'queued';
    const filtered = g_shows.filter(s => (s.queue === 'queued') === isQueued);
    sortShowArray(filtered);
    const ids = filtered.map(s => s.id);
    const dragIdx = ids.indexOf(g_drag_id);
    if (dragIdx < 0) return;

    ids.splice(dragIdx, 1);
    let dropIdx = ids.indexOf(targetId);
    if (dropIdx < 0) return;
    if (insertAfter) dropIdx += 1;
    ids.splice(dropIdx, 0, g_drag_id);

    ids.forEach((id, i) => {
        const show = g_shows.find(s => s.id === id);
        if (show) show.sort_order = i + 1;
    });
    renderShows(g_shows);
    api('PUT', '/api/shows/reorder', { order: ids });
}

function movieDrop(event, targetId) {
    event.preventDefault();
    const targetWrap = event.currentTarget;
    const insertAfter = targetWrap.classList.contains('drag-over-after');
    document.querySelectorAll('.drag-over-before, .drag-over-after')
        .forEach(el => { el.classList.remove('drag-over-before', 'drag-over-after'); });
    if (!g_drag_id || g_drag_id === targetId || g_drag_type !== 'movie') return;

    sortMovieArray(g_movies);
    const ids = g_movies.map(m => m.id);
    const dragIdx = ids.indexOf(g_drag_id);
    if (dragIdx < 0) return;

    ids.splice(dragIdx, 1);
    let dropIdx = ids.indexOf(targetId);
    if (dropIdx < 0) return;
    if (insertAfter) dropIdx += 1;
    ids.splice(dropIdx, 0, g_drag_id);

    ids.forEach((id, i) => {
        const movie = g_movies.find(m => m.id === id);
        if (movie) movie.sort_order = i + 1;
    });
    renderMovies(g_movies);
    api('PUT', '/api/movies/reorder', { order: ids });
}

function listDragEdge(event, type) {
    const listId = type === 'show' ? 'shows-list' : 'movies-list';
    const sel = type === 'show' ? '.show-wrap:not(.dragging)' : '.movie-wrap:not(.dragging)';
    const items = document.getElementById(listId).querySelectorAll(sel);
    if (!items.length) return null;
    const firstRect = items[0].getBoundingClientRect();
    const lastRect  = items[items.length - 1].getBoundingClientRect();
    const atStart = event.clientY < firstRect.top ||
        (event.clientY < firstRect.bottom && event.clientX < firstRect.left);
    if (atStart) return { el: items[0], cls: 'drag-over-before', pos: 'start' };
    return { el: items[items.length - 1], cls: 'drag-over-after', pos: 'end' };
}

function listDragOver(event, type) {
    if (!g_drag_id || g_drag_type !== type) return;
    const wrap = event.target.closest('.show-wrap, .movie-wrap');
    if (wrap) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = 'move';
    document.querySelectorAll('.drag-over-before, .drag-over-after')
        .forEach(el => { el.classList.remove('drag-over-before', 'drag-over-after'); });
    const edge = listDragEdge(event, type);
    if (edge) edge.el.classList.add(edge.cls);
}

function listDrop(event, type) {
    if (!g_drag_id || g_drag_type !== type) return;
    const wrap = event.target.closest('.show-wrap, .movie-wrap');
    if (wrap) return;
    event.preventDefault();
    const edge = listDragEdge(event, type);
    document.querySelectorAll('.drag-over-before, .drag-over-after')
        .forEach(el => { el.classList.remove('drag-over-before', 'drag-over-after'); });
    const atStart = edge && edge.pos === 'start';
    if (type === 'show') {
        const isQueued = g_shows_tab === 'queued';
        const filtered = g_shows.filter(s => (s.queue === 'queued') === isQueued);
        sortShowArray(filtered);
        const ids = filtered.map(s => s.id);
        const dragIdx = ids.indexOf(g_drag_id);
        if (dragIdx < 0) return;
        ids.splice(dragIdx, 1);
        if (atStart) ids.unshift(g_drag_id); else ids.push(g_drag_id);
        ids.forEach((id, i) => {
            const show = g_shows.find(s => s.id === id);
            if (show) show.sort_order = i + 1;
        });
        renderShows(g_shows);
        api('PUT', '/api/shows/reorder', { order: ids });
    } else {
        sortMovieArray(g_movies);
        const ids = g_movies.map(m => m.id);
        const dragIdx = ids.indexOf(g_drag_id);
        if (dragIdx < 0) return;
        ids.splice(dragIdx, 1);
        if (atStart) ids.unshift(g_drag_id); else ids.push(g_drag_id);
        ids.forEach((id, i) => {
            const movie = g_movies.find(m => m.id === id);
            if (movie) movie.sort_order = i + 1;
        });
        renderMovies(g_movies);
        api('PUT', '/api/movies/reorder', { order: ids });
    }
}

// ---------- Shows — render -----------------------------------------------

// ---------- Main / sub tabs -----------------------------------------------

let g_main_tab  = 'shows';   // 'shows' | 'movies'
let g_shows_tab = 'current'; // 'current' | 'queued'
let g_shows     = [];        // last loaded shows, for sub-tab re-render
let g_movies    = [];        // last loaded movies, for drag reorder
let g_queues    = [];
let g_queue_id  = parseInt(localStorage.getItem('fi_queue_id')) || 1;

function switchMainTab(tab) {
    g_main_tab = tab;
    document.getElementById('tab-shows').classList.toggle('active', tab === 'shows');
    document.getElementById('tab-movies').classList.toggle('active', tab === 'movies');
    const calTab = document.getElementById('tab-calendar');
    if (calTab) calTab.classList.toggle('active', tab === 'calendar');

    document.getElementById('shows-section').classList.toggle('hidden', tab !== 'shows');
    document.getElementById('movies-section').classList.toggle('hidden', tab !== 'movies');
    const calSec = document.getElementById('calendar-section');
    if (calSec) calSec.classList.toggle('hidden', tab !== 'calendar');

    if (tab === 'calendar') {
        loadCalendar(true);
    }
}

function switchShowsTab(tab) {
    g_shows_tab = tab;
    document.getElementById('subtab-current').classList.toggle('active', tab === 'current');
    document.getElementById('subtab-queued').classList.toggle('active', tab === 'queued');
    renderShows(g_shows);
}

async function loadShows() {
    const shows = await api('GET', `/api/shows?queue_id=${g_queue_id}`);
    g_shows = shows;
    renderShows(shows);
}

function showStatusBadge(s) {
    if (s.status === 'finished')
        return '<span class="badge badge-finished">Finished</span>';
    if (s.season === 0)
        return '<span class="badge badge-notstarted">Not started</span>';
    if (showHasNew(s))
        return '';
    return '<span class="badge badge-caughtup">Caught up</span>';
}

function showCard(s) {
    const notesHtml    = s.notes
        ? `<div class="card-notes">${esc(s.notes)}</div>` : '';
    const thumbImg     = s.thumbnail_url
        ? `<img class="card-thumb" src="${esc(s.thumbnail_url)}" alt="" loading="lazy">` : '';
    const thumbHtml    = (thumbImg && s.imdb_id)
        ? `<a href="https://www.imdb.com/title/${esc(s.imdb_id)}/" target="_blank" rel="noopener">${thumbImg}</a>`
        : thumbImg;
    const titleJson    = JSON.stringify(s.title).replace(/"/g, '&quot;');
    const hasNew       = showHasNew(s);
    const newBadge     = hasNew ? '<span class="badge badge-new">NEW</span>' : '';
    const nextCls      = hasNew ? 'ep-next-watch ep-next-watch-new' : 'ep-next-watch';
    const nextWatchTxt = nextTxt(s).replace(' − ', ' &minus; ');

    return `
    <div class="show-wrap" id="wrap-${s.id}"
         draggable="true"
         ondragstart="itemDragStart(event,'show',${s.id})"
         ondragend="itemDragEnd(event)"
         ondragover="itemDragOver(event)"
         ondragleave="itemDragLeave(event)"
         ondrop="showDrop(event,${s.id})">

      <!-- ── Main card ──────────────────────────────────────────────── -->
      <div class="card" data-id="${s.id}">
        <div class="drag-handle" onmousedown="dragHandleDown(event)"
             ontouchstart="dragHandleDown(event)" title="Drag to reorder">&#x2630;</div>
        ${thumbHtml}
        <div class="card-body">
          <div class="card-top">
            <div class="card-title">
              <a class="show-title-link" href="#"
                 onclick="event.preventDefault();openEpisodeView(${s.id},${titleJson})">${esc(s.title)}</a>
            </div>
            ${showStatusBadge(s)}${newBadge}
          </div>
          ${s.service ? `<div class="card-meta"><span class="service-tag">${esc(s.service)}</span></div>` : ''}
          <div class="ep-track">
            <div class="ep-last-watched"
                 onclick="openWatchedPopup(${s.id}, ${titleJson}, ${Math.max(s.season, 1)})">
              ${s.season === 0
                ? 'Last watched: <em>Not started</em>'
                : `Last watched: S${s.season} &minus; E${s.episode}`}
            </div>
            ${s.season === 0 ? '' : `<div class="${nextCls}">${nextWatchTxt}</div>`}
          </div>
          ${notesHtml}
          <div class="card-actions">
            <button class="btn-sm" onclick="toggleEdit(${s.id})">Edit</button>
            <button class="btn-sm" onclick="openCastModal('shows',${s.id},${titleJson})">Cast</button>
            <button class="btn-sm" onclick="toggleShowQueue(${s.id},'${s.queue}')">
              ${s.queue === 'queued' ? '→ Current' : '→ Queued'}
            </button>
            ${moveToQueueHtml('shows', s.id)}
            <button class="btn-danger" onclick="deleteShow(${s.id})">Remove</button>
          </div>
        </div>
      </div>

      <!-- ── Edit panel ─────────────────────────────────────────────── -->
      <div class="edit-panel hidden" id="edit-${s.id}">
        <div class="form-grid">
          <div class="field span2">
            <label>Title</label>
            <input type="text" id="etitle-${s.id}" value="${esc(s.title)}" autocomplete="off">
          </div>
          <div class="field">
            <label>Service</label>
            <input type="text" id="eservice-${s.id}" value="${esc(s.service)}"
                   list="services-list" autocomplete="off">
          </div>
          <div class="field">
            <label>IMDB ID</label>
            <input type="text" id="eimdb-${s.id}" value="${esc(s.imdb_id)}"
                   placeholder="tt0903747" autocomplete="off">
          </div>
          <div class="field">
            <label>Last Season / Episode Watched</label>
            <input type="text" id="eep-${s.id}" value="${ep(s.season, s.episode)}"
                   placeholder="s001-e001" autocomplete="off">
          </div>
          <div class="field">
            <label>Notes</label>
            <input type="text" id="enotes-${s.id}" value="${esc(s.notes)}" autocomplete="off">
          </div>
        </div>
        <div class="form-actions">
          <button class="btn-primary" onclick="saveEdit(${s.id})">Save</button>
          <button class="btn-cancel"  onclick="closeEdit(${s.id})">Cancel</button>
        </div>
      </div>

    </div>`;
}

function nextTxt(s) {
    if (s.next_season > 0) {
        const base = `Next: S${s.next_season} − E${s.next_episode}`;
        return s.next_episode_title ? `${base} — ${s.next_episode_title}` : base;
    }
    if (s.season_episodes > 0 && s.episode >= s.season_episodes)
        return `Next: S${s.season + 1} − E1`;
    return `Next: S${s.season} − E${s.episode + 1}`;
}

function showHasNew(s) {
    if (s.latest_season <= 0) return false;
    if (s.season === 0) return true;  // not started, anything aired is new
    return s.latest_season > s.season ||
        (s.latest_season === s.season && s.latest_episode > s.episode);
}

function sortShowArray(arr) {
    const rank = s => {
        if (s.status === 'finished') return 2;
        if (showHasNew(s))           return 0;
        return 1;
    };
    arr.sort((a, b) => {
        const so_a = a.sort_order || 999999;
        const so_b = b.sort_order || 999999;
        if (so_a !== so_b) return so_a - so_b;
        const d = rank(a) - rank(b);
        return d !== 0 ? d : a.title.localeCompare(b.title);
    });
}

function renderShows(shows) {
    const list = document.getElementById('shows-list');
    const filtered = shows.filter(s => (s.queue === 'queued') === (g_shows_tab === 'queued'));
    if (!filtered.length) {
        list.innerHTML = g_shows_tab === 'queued'
            ? '<div class="empty">No queued shows.</div>'
            : '<div class="empty">No shows yet — add one above.</div>';
        return;
    }
    sortShowArray(filtered);
    list.innerHTML = filtered.map(showCard).join('');
}

// ---------- Shows — card actions -----------------------------------------

async function deleteShow(id) {
    if (!confirm('Remove this show?')) return;
    await api('DELETE', `/api/shows/${id}`);
    loadShows();
}

async function toggleShowQueue(id, current) {
    const next = current === 'queued' ? 'current' : 'queued';
    await api('PUT', `/api/shows/${id}`, { queue: next });
    loadShows();
}

// ---------- Shows — edit panel -------------------------------------------

function toggleEdit(id) {
    const wrap  = document.getElementById(`wrap-${id}`);
    const panel = document.getElementById(`edit-${id}`);
    const opening = panel.classList.contains('hidden');
    panel.classList.toggle('hidden', !opening);
    wrap.classList.toggle('panel-open', opening);
}

function closeEdit(id) {
    document.getElementById(`edit-${id}`).classList.add('hidden');
    document.getElementById(`wrap-${id}`).classList.remove('panel-open');
}

async function saveEdit(id) {
    const title = document.getElementById(`etitle-${id}`).value.trim();
    if (!title) { alert('Title is required.'); return; }

    const rawEp = document.getElementById(`eep-${id}`).value.trim();
    const pos   = rawEp ? parseEp(rawEp) : null;
    if (rawEp && !pos) { alert('Episode must be in format s001-e001'); return; }

    const body = {
        title,
        service:  document.getElementById(`eservice-${id}`).value.trim(),
        imdb_id:  document.getElementById(`eimdb-${id}`).value.trim(),
        notes:    document.getElementById(`enotes-${id}`).value.trim(),
    };
    if (pos) { body.season = pos.season; body.episode = pos.episode; }

    await api('PUT', `/api/shows/${id}`, body);
    loadShows();
}

// ---------- Shows — add form ---------------------------------------------

function showAddShowForm() {
    document.getElementById('add-show-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    document.getElementById('new-show-title').focus();
}
function hideAddShowForm() {
    document.getElementById('add-show-modal').classList.add('hidden');
    document.body.style.overflow = '';
    clearShowForm();
}
function addShowModalClick(event) {
    if (event.target === document.getElementById('add-show-modal'))
        hideAddShowForm();
}
function clearShowForm() {
    ['new-show-title', 'new-show-service', 'new-show-imdb', 'new-show-ep']
        .forEach(id => { document.getElementById(id).value = ''; });
    document.getElementById('show-search-results').classList.add('hidden');
    g_new_show_tmdb_id = 0;
    g_new_show_thumb   = '';
}

async function addShow() {
    const title = document.getElementById('new-show-title').value.trim();
    if (!title) { alert('Title is required.'); return; }

    const raw = document.getElementById('new-show-ep').value.trim() || 's000-e000';
    const pos  = parseEp(raw);
    if (!pos) { alert('Use the format s001-e001'); return; }

    await api('POST', '/api/shows', {
        title,
        service:       document.getElementById('new-show-service').value.trim(),
        season:        pos.season,
        episode:       pos.episode,
        imdb_id:       document.getElementById('new-show-imdb').value.trim(),
        tmdb_id:       g_new_show_tmdb_id,
        thumbnail_url: g_new_show_thumb,
        queue_id:      g_queue_id,
    });
    hideAddShowForm();
    loadShows();
}

// ---------- Season / episode popup ----------------------------------------

let g_popup_show_id = 0;
let g_popup_season  = 0;

async function openWatchedPopup(showId, title, currentSeason) {
    g_popup_show_id = showId;
    g_popup_season  = currentSeason;

    document.getElementById('ep-modal-title').textContent = title;
    document.getElementById('ep-seasons-pane').innerHTML =
        '<div class="empty">Loading…</div>';
    document.getElementById('ep-episodes-pane').innerHTML = '';
    document.getElementById('ep-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';

    try {
        const seasons = await api('GET', `/api/shows/${showId}/seasons`);
        if (seasons.error) {
            document.getElementById('ep-seasons-pane').innerHTML =
                `<div class="empty">${esc(seasons.error)}</div>`;
            return;
        }
        renderSeasons(showId, seasons, currentSeason);
        await loadPopupEpisodes(showId, currentSeason);
    } catch {
        document.getElementById('ep-seasons-pane').innerHTML =
            '<div class="empty">Failed to load seasons.</div>';
    }
}

function closeWatchedPopup() {
    document.getElementById('ep-modal').classList.add('hidden');
    document.body.style.overflow = '';
    g_popup_show_id = 0;
    g_popup_season  = 0;
}

function modalOverlayClick(event) {
    if (event.target === document.getElementById('ep-modal'))
        closeWatchedPopup();
}

function seasonClass(watched, total) {
    if (total === 0)      return 'season-unknown';
    if (watched >= total) return 'season-done';
    if (watched > 0)      return 'season-partial';
    return 'season-none';
}

function renderSeasons(showId, seasons, activeSeason,
                       paneId = 'ep-seasons-pane', clickFn = 'selectSeason') {
    const pane = document.getElementById(paneId);
    if (!pane) return;
    if (!seasons.length) {
        pane.innerHTML = '<div class="empty">No seasons found.</div>';
        return;
    }
    pane.innerHTML = seasons.map(s => `
      <button class="season-btn ${seasonClass(s.watched_count, s.total_episodes)}${s.season === activeSeason ? ' season-active' : ''}"
              id="sbtn-${showId}-${s.season}"
              onclick="${clickFn}(${showId}, ${s.season})">
        S${String(s.season).padStart(2,'0')}
        <span class="season-ep-count">${s.watched_count}/${s.total_episodes}</span>
      </button>`).join('');
}

async function episodeCheckAll(showId, season, checkbox) {
    const watched = checkbox.checked;
    checkbox.disabled = true;
    try {
        const episodes = await api('GET', `/api/shows/${showId}/episodes?season=${season}`);
        if (!Array.isArray(episodes) || episodes.error) return;
        const seasonTotal = episodes.length;
        await Promise.all(
            episodes.map(ep =>
                api('PUT', `/api/shows/${showId}/episodes/${season}/${ep.episode}/watched`,
                    {watched, season_total: seasonTotal})
            )
        );
        const [seasons] = await Promise.all([
            api('GET', `/api/shows/${showId}/seasons`),
            loadShows(),
        ]);
        if (!seasons.error) renderSeasons(showId, seasons, g_popup_season);
        if (season === g_popup_season) await loadPopupEpisodes(showId, season);
    } finally {
        checkbox.disabled = false;
    }
}

async function selectSeason(showId, season) {
    g_popup_season = season;
    document.querySelectorAll('.season-btn').forEach(b => b.classList.remove('season-active'));
    const btn = document.getElementById(`sbtn-${showId}-${season}`);
    if (btn) btn.classList.add('season-active');
    await loadPopupEpisodes(showId, season);
}

async function loadPopupEpisodes(showId, season) {
    const pane = document.getElementById('ep-episodes-pane');
    pane.innerHTML = '<div class="empty">Fetching episodes…</div>';
    try {
        const episodes = await api('GET', `/api/shows/${showId}/episodes?season=${season}`);
        if (episodes.error) {
            pane.innerHTML = `<div class="empty">${esc(episodes.error)}</div>`;
            return;
        }
        renderPopupEpisodes(showId, season, episodes);
    } catch {
        pane.innerHTML = '<div class="empty">Failed to load episodes.</div>';
    }
}

function renderPopupEpisodes(showId, season, episodes) {
    const pane = document.getElementById('ep-episodes-pane');
    if (!Array.isArray(episodes) || !episodes.length) {
        pane.innerHTML = '<div class="empty">No episodes found.</div>';
        return;
    }
    const seasonTotal  = episodes.length;
    const watchedCount = episodes.filter(e => e.watched).length;
    const allWatched   = watchedCount === seasonTotal;
    const someWatched  = watchedCount > 0 && !allWatched;

    pane.innerHTML = `
      <div class="ep-all-row">
        <input type="checkbox" id="ep-all-${showId}-${season}" class="ep-all-check"
               ${allWatched ? 'checked' : ''}
               onchange="episodeCheckAll(${showId},${season},this)">
        <label for="ep-all-${showId}-${season}">All</label>
      </div>` +
    episodes.map(e => `
      <div class="popup-ep-row${e.watched ? ' ep-watched' : ''}"
           id="pep-${showId}-${e.season}-${e.episode}">
        <input type="checkbox" ${e.watched ? 'checked' : ''}
               onchange="popupMarkWatched(${showId},${e.season},${e.episode},this.checked,${seasonTotal})">
        <span class="ep-code">E${String(e.episode).padStart(3,'0')}</span>
        <span class="ep-title">${esc(e.title || '—')}</span>
        <span class="ep-date">${e.air_date || ''}</span>
      </div>`).join('');

    const allCb = document.getElementById(`ep-all-${showId}-${season}`);
    if (allCb) allCb.indeterminate = someWatched;
}

async function popupMarkWatched(showId, season, episode, watched, seasonTotal = 0) {
    const data = await api('PUT',
        `/api/shows/${showId}/episodes/${season}/${episode}/watched`,
        {watched, season_total: seasonTotal});

    const row = document.getElementById(`pep-${showId}-${season}-${episode}`);
    if (row) row.classList.toggle('ep-watched', watched);

    if (data.show) {
        const show = data.show;
        const wrap = document.getElementById(`wrap-${showId}`);
        if (wrap) {
            const lw = wrap.querySelector('.ep-last-watched');
            const nw = wrap.querySelector('.ep-next-watch');
            if (lw) lw.innerHTML = show.season === 0
                ? 'Last watched: <em>Not started</em>'
                : `Last watched: S${pad3(show.season)} − E${pad3(show.episode)}`;
            const epTrack = wrap.querySelector('.ep-track');
            if (show.season === 0) {
                if (nw) nw.remove();
            } else if (nw) {
                nw.textContent = nextTxt(show);
            } else if (epTrack) {
                const d = document.createElement('div');
                d.className = 'ep-next-watch';
                d.textContent = nextTxt(show);
                epTrack.appendChild(d);
            }
        }
    }

    // Sync the "all" checkbox in the episode pane
    const epRows = document.querySelectorAll('#ep-episodes-pane .popup-ep-row input[type=checkbox]');
    if (epRows.length) {
        const checkedCount = Array.from(epRows).filter(cb => cb.checked).length;
        const allCb = document.getElementById(`ep-all-${showId}-${season}`);
        if (allCb) {
            allCb.checked       = checkedCount === epRows.length;
            allCb.indeterminate = checkedCount > 0 && checkedCount < epRows.length;
        }
    }

    const seasons = await api('GET', `/api/shows/${showId}/seasons`);
    if (!seasons.error) renderSeasons(showId, seasons, g_popup_season);
}

// ---------- Movies --------------------------------------------------------

async function loadMovies() {
    const movies = await api('GET', `/api/movies?queue_id=${g_queue_id}`);
    g_movies = movies;
    renderMovies(movies);
}

function movieStatusBadge(id, status) {
    const map = {
        want_to_watch: ['badge-want',    'Want to watch'],
        watched:       ['badge-watched', 'Watched'],
    };
    const [cls, label] = map[status] ?? ['badge-want', status];
    return `<span class="badge badge-toggle ${cls}"
                  onclick="toggleWatched(${id},'${status}')">${label}</span>`;
}

function movieCard(m) {
    const notesHtml  = m.notes
        ? `<div class="card-notes">${esc(m.notes)}</div>` : '';
    let dateHtml;
    if (m.release_date) {
        const today = new Date(); today.setHours(0, 0, 0, 0);
        const rel   = new Date(m.release_date + 'T00:00:00');
        const lbl   = rel > today ? 'Releases:' : 'Released:';
        dateHtml = `<div class="card-meta"><span class="release-date">${lbl} ${esc(m.release_date)}</span></div>`;
    } else {
        dateHtml = `<div class="card-meta"><span class="release-date release-date-unknown">No release date yet.</span></div>`;
    }
    const thumbImg   = m.thumbnail_url
        ? `<img class="card-thumb" src="${esc(m.thumbnail_url)}" alt="" loading="lazy">` : '';
    const thumbHtml  = (thumbImg && m.imdb_id)
        ? `<a href="https://www.imdb.com/title/${esc(m.imdb_id)}/" target="_blank" rel="noopener">${thumbImg}</a>`
        : thumbImg;
    const mTmdbUrl     = m.tmdb_id ? `https://www.themoviedb.org/movie/${m.tmdb_id}` : '';
    const movieTitleHtml = (m.imdb_id || m.tmdb_id)
        ? `<span class="movie-title-link"
                 data-tmdb="${esc(mTmdbUrl)}"
                 data-imdb="${esc(m.imdb_id || '')}"
                 onclick="openEpLinkPopup(event,this.dataset.tmdb,this.dataset.imdb)"
           >${esc(m.title)}</span>`
        : esc(m.title);

    return `
    <div class="movie-wrap" id="mwrap-${m.id}"
         draggable="true"
         ondragstart="itemDragStart(event,'movie',${m.id})"
         ondragend="itemDragEnd(event)"
         ondragover="itemDragOver(event)"
         ondragleave="itemDragLeave(event)"
         ondrop="movieDrop(event,${m.id})">
    <div class="card" data-id="${m.id}">
      <div class="drag-handle" onmousedown="dragHandleDown(event)"
           ontouchstart="dragHandleDown(event)" title="Drag to reorder">&#x2630;</div>
      ${thumbHtml}
      <div class="card-body">
      <div class="card-top">
        <div class="card-title">${movieTitleHtml}</div>
        ${movieStatusBadge(m.id, m.status)}
      </div>
      ${dateHtml}
      ${notesHtml}
      <div class="card-actions">
        <button class="btn-sm" onclick="openCastModal('movies',${m.id},${JSON.stringify(m.title).replace(/"/g,'&quot;')})">Cast</button>
        ${moveToQueueHtml('movies', m.id)}
        <button class="btn-danger" onclick="deleteMovie(${m.id})">Remove</button>
      </div>
      </div>
    </div>
    </div>`;
}

function sortMovieArray(arr) {
    arr.sort((a, b) => {
        const so_a = a.sort_order || 999999;
        const so_b = b.sort_order || 999999;
        if (so_a !== so_b) return so_a - so_b;
        return a.title.localeCompare(b.title);
    });
}

function renderMovies(movies) {
    const list = document.getElementById('movies-list');
    if (!movies.length) {
        list.innerHTML = '<div class="empty">No movies yet — add one above.</div>';
        return;
    }
    sortMovieArray(movies);
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
    document.getElementById('add-movie-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    document.getElementById('new-movie-title').focus();
}
function hideAddMovieForm() {
    document.getElementById('add-movie-modal').classList.add('hidden');
    document.body.style.overflow = '';
    clearMovieForm();
}
function addMovieModalClick(event) {
    if (event.target === document.getElementById('add-movie-modal'))
        hideAddMovieForm();
}
function clearMovieForm() {
    ['new-movie-title','new-movie-imdb','new-movie-notes']
        .forEach(id => { document.getElementById(id).value = ''; });
    document.getElementById('movie-search-results').classList.add('hidden');
    g_new_movie_tmdb_id = 0;
    g_new_movie_thumb   = '';
}

async function addMovie() {
    const title = document.getElementById('new-movie-title').value.trim();
    if (!title) { alert('Title is required.'); return; }
    await api('POST', '/api/movies', {
        title,
        imdb_id:       document.getElementById('new-movie-imdb').value.trim(),
        notes:         document.getElementById('new-movie-notes').value.trim(),
        tmdb_id:       g_new_movie_tmdb_id,
        thumbnail_url: g_new_movie_thumb,
        queue_id:      g_queue_id,
    });
    hideAddMovieForm();
    loadMovies();
}

// ---------- Episode browser view ------------------------------------------

let g_ev_show_id = 0;
let g_ev_season  = 0;

function titleClick() {
    if (g_ev_show_id) closeEpisodeView();
}

async function openEpisodeView(showId, title) {
    g_ev_show_id = showId;
    document.getElementById('ev-title').textContent = title;
    document.getElementById('ev-seasons-pane').innerHTML = '<div class="empty">Loading…</div>';
    document.getElementById('ev-episodes-pane').innerHTML = '<div class="empty">Select a season</div>';
    document.getElementById('main-view').classList.add('hidden');
    document.getElementById('episode-view').classList.remove('hidden');
    document.getElementById('site-title').classList.add('title-nav');

    const seasons = await api('GET', `/api/shows/${showId}/seasons`);
    if (!seasons.error)
        renderSeasons(showId, seasons, 0, 'ev-seasons-pane', 'selectEpisodeViewSeason');
}

function closeEpisodeView() {
    document.getElementById('episode-view').classList.add('hidden');
    document.getElementById('main-view').classList.remove('hidden');
    document.getElementById('site-title').classList.remove('title-nav');
    g_ev_show_id = 0;
    g_ev_season  = 0;
    loadShows();  // refresh card in case watched state changed
}

async function selectEpisodeViewSeason(showId, season) {
    g_ev_season = season;
    document.querySelectorAll('#ev-seasons-pane .season-btn').forEach(b => b.classList.remove('season-active'));
    const btn = document.getElementById(`sbtn-${showId}-${season}`);
    if (btn) btn.classList.add('season-active');

    const pane = document.getElementById('ev-episodes-pane');
    pane.innerHTML = '<div class="empty">Loading episodes…</div>';
    const episodes = await api('GET', `/api/shows/${showId}/episodes?season=${season}`);
    if (!Array.isArray(episodes) || episodes.error) {
        pane.innerHTML = `<div class="empty">${esc(episodes?.error || 'Failed to load')}</div>`;
        return;
    }
    renderEpisodeCards(showId, season, episodes);
}

function renderEpisodeCards(showId, season, episodes) {
    const pane = document.getElementById('ev-episodes-pane');
    if (!episodes.length) { pane.innerHTML = '<div class="empty">No episodes found.</div>'; return; }
    const seasonTotal = episodes.length;
    pane.innerHTML = episodes.map(ep => {
        const epTitleJson = JSON.stringify(ep.title || '').replace(/"/g, '&quot;');
        return `
        <div class="ep-card${ep.watched ? ' ep-card-watched' : ''}"
             id="epcard-${showId}-${season}-${ep.episode}">
          <div class="ep-card-body">
            <div class="ep-card-heading">
              <span class="ep-card-num">E${String(ep.episode).padStart(3,'0')}</span>
              <span class="ep-card-title ep-card-title-link"
                    data-tmdb="${esc(ep.episode_url || '')}"
                    data-imdb="${esc(ep.imdb_id || '')}"
                    onclick="openEpLinkPopup(event,this.dataset.tmdb,this.dataset.imdb)"
              >${esc(ep.title || '—')}</span>
            </div>
            ${ep.air_date ? `<div class="ep-card-date">${esc(ep.air_date)}</div>` : ''}
          </div>
          <div class="ep-card-actions">
            <label class="ep-watched-toggle">
              <input type="checkbox" ${ep.watched ? 'checked' : ''}
                     onchange="evToggleWatched(${showId},${season},${ep.episode},this.checked,${seasonTotal})">
              <span>Watched</span>
            </label>
            <button class="btn-sm" onclick="openEpisodeCast(${showId},${season},${ep.episode},${epTitleJson})">Cast</button>
          </div>
        </div>`;
    }).join('');
}

async function evToggleWatched(showId, season, episode, watched, seasonTotal) {
    await api('PUT', `/api/shows/${showId}/episodes/${season}/${episode}/watched`,
              {watched, season_total: seasonTotal});
    const card = document.getElementById(`epcard-${showId}-${season}-${episode}`);
    if (card) card.classList.toggle('ep-card-watched', watched);
    // Refresh season progress counts in the left pane
    const seasons = await api('GET', `/api/shows/${showId}/seasons`);
    if (!seasons.error)
        renderSeasons(showId, seasons, g_ev_season, 'ev-seasons-pane', 'selectEpisodeViewSeason');
}

async function openEpisodeCast(showId, season, episode, epTitle) {
    const label = `S${String(season).padStart(2,'0')}E${String(episode).padStart(3,'0')}`
                + (epTitle ? ` — ${epTitle}` : '');
    document.getElementById('cast-modal-title').textContent = label + ' — Cast';
    document.getElementById('cast-modal-body').innerHTML =
        '<div class="empty">Loading cast… (first load may take a moment)</div>';
    document.getElementById('cast-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    try {
        const cast = await api('GET', `/api/shows/${showId}/episodes/${season}/${episode}/cast`);
        renderCast(cast);
    } catch {
        document.getElementById('cast-modal-body').innerHTML =
            '<div class="empty">Failed to load cast.</div>';
    }
}

// ---------- Cast modal ----------------------------------------------------

async function openCastModal(type, id, title) {
    document.getElementById('cast-modal-title').textContent = title + ' — Cast';
    document.getElementById('cast-modal-body').innerHTML =
        '<div class="empty">Loading cast… (first load may take a moment)</div>';
    document.getElementById('cast-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    try {
        const cast = await api('GET', `/api/${type}/${id}/cast`);
        renderCast(cast);
    } catch {
        document.getElementById('cast-modal-body').innerHTML =
            '<div class="empty">Failed to load cast.</div>';
    }
}

function renderCast(cast) {
    const body = document.getElementById('cast-modal-body');
    if (!Array.isArray(cast) || !cast.length) {
        body.innerHTML = '<div class="empty">No cast data available.</div>';
        return;
    }
    body.innerHTML = cast.map(cm => {
        const nameHtml = cm.imdb_id
            ? `<a href="https://www.imdb.com/name/${esc(cm.imdb_id)}/" target="_blank" rel="noopener" class="cast-name">${esc(cm.name)}</a>`
            : `<span class="cast-name">${esc(cm.name)}</span>`;
        const photo = cm.profile_url
            ? `<img class="cast-photo" src="${esc(cm.profile_url)}" alt="" loading="lazy">`
            : `<div class="cast-photo-placeholder"></div>`;
        return `<div class="cast-row">${photo}<div class="cast-info">${nameHtml}<div class="cast-character">${esc(cm.character || '—')}</div></div></div>`;
    }).join('');
}

function closeCastModal() {
    document.getElementById('cast-modal').classList.add('hidden');
    document.body.style.overflow = '';
}

function castModalClick(event) {
    if (event.target === document.getElementById('cast-modal')) closeCastModal();
}

// ---------- Zoom ---------------------------------------------------------

const ZOOM_MIN = 0.7, ZOOM_MAX = 1.5, ZOOM_STEP = 0.1;
let g_zoom = parseFloat(localStorage.getItem('fi_zoom')) || 1;
applyZoom();

function applyZoom() {
    document.body.style.zoom = g_zoom;
    const label = document.getElementById('zoom-label');
    if (label) label.textContent = Math.round(g_zoom * 100) + '%';
    updateZoomCols();
}
function updateZoomCols() {
    const ew = window.innerWidth / g_zoom;
    document.body.classList.toggle('zoom-cols-3', ew >= 1100 && ew < 1500);
    document.body.classList.toggle('zoom-cols-4', ew >= 1500);
}
function zoomIn()  { g_zoom = Math.min(ZOOM_MAX, +(g_zoom + ZOOM_STEP).toFixed(2)); localStorage.setItem('fi_zoom', g_zoom); applyZoom(); }
function zoomOut() { g_zoom = Math.max(ZOOM_MIN, +(g_zoom - ZOOM_STEP).toFixed(2)); localStorage.setItem('fi_zoom', g_zoom); applyZoom(); }
window.addEventListener('resize', updateZoomCols);

// ---------- Init ----------------------------------------------------------

// ---------- Header menu ---------------------------------------------------

function toggleMenu() {
    document.getElementById('menu-dropdown').classList.toggle('hidden');
}

document.addEventListener('click', e => {
    const menu = document.getElementById('header-menu');
    if (menu && !menu.contains(e.target))
        document.getElementById('menu-dropdown').classList.add('hidden');
});

// ---------- Check All modal -----------------------------------------------

let g_check_es = null;

function openCheckModal() {
    document.getElementById('menu-dropdown').classList.add('hidden');
    const log    = document.getElementById('check-log');
    const status = document.getElementById('check-status');
    log.textContent  = '';
    status.textContent = 'Running…';
    status.className   = 'check-status running';
    document.getElementById('check-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    if (g_check_es) { g_check_es.close(); g_check_es = null; }
    g_check_es = new EventSource('/api/check/stream');
    g_check_es.onmessage = e => {
        const body = document.querySelector('.check-log-body');
        log.textContent += e.data + '\n';
        body.scrollTop = body.scrollHeight;
    };
    g_check_es.addEventListener('done', () => {
        g_check_es.close();
        g_check_es = null;
        const status = document.getElementById('check-status');
        const m = document.getElementById('check-log').textContent
            .match(/(\d+) with new episode\(s\)/);
        const newCount = m ? parseInt(m[1], 10) : 0;
        if (newCount > 0) {
            status.textContent = newCount + ' NEW';
            status.className   = 'check-status has-new';
        } else {
            status.textContent = 'Done';
            status.className   = 'check-status done';
        }
        loadShows();
        loadMovies();
    });
    g_check_es.onerror = () => {
        if (g_check_es) { g_check_es.close(); g_check_es = null; }
        const status = document.getElementById('check-status');
        if (status.className === 'check-status running') {
            status.textContent = 'Error';
            status.className   = 'check-status';
        }
    };
}

function closeCheckModal() {
    if (g_check_es) { g_check_es.close(); g_check_es = null; }
    document.getElementById('check-modal').classList.add('hidden');
    document.body.style.overflow = '';
}

function checkModalOverlayClick(event) {
    if (event.target === document.getElementById('check-modal'))
        closeCheckModal();
}

// ---------- Episode link popup --------------------------------------------

function openEpLinkPopup(event, tmdbUrl, imdbId) {
    event.stopPropagation();
    const popup  = document.getElementById('ep-link-popup');
    const tmdbEl = document.getElementById('ep-link-tmdb');
    const imdbEl = document.getElementById('ep-link-imdb');

    if (tmdbUrl) {
        tmdbEl.href = tmdbUrl;
        tmdbEl.classList.remove('ep-link-btn-disabled');
    } else {
        tmdbEl.href = '#';
        tmdbEl.classList.add('ep-link-btn-disabled');
    }
    if (imdbId) {
        imdbEl.href = `https://www.imdb.com/title/${imdbId}/`;
        imdbEl.classList.remove('ep-link-btn-disabled');
    } else {
        imdbEl.href = '#';
        imdbEl.classList.add('ep-link-btn-disabled');
    }

    const rect = event.currentTarget.getBoundingClientRect();
    popup.style.top  = (rect.bottom + 4) + 'px';
    popup.style.left = rect.left + 'px';
    popup.classList.remove('hidden');
}

function closeEpLinkPopup() {
    document.getElementById('ep-link-popup').classList.add('hidden');
}

document.addEventListener('click', e => {
    if (!document.getElementById('ep-link-popup').classList.contains('hidden'))
        closeEpLinkPopup();
});

document.addEventListener('keydown', e => {
    if (e.key === 'Escape') closeEpLinkPopup();
    if (e.key === 'Escape' && g_popup_show_id) closeWatchedPopup();
    if (e.key === 'Escape' && !document.getElementById('check-modal').classList.contains('hidden'))
        closeCheckModal();
    if (e.key === 'Escape' && !document.getElementById('cast-modal').classList.contains('hidden'))
        closeCastModal();
    if (e.key === 'Escape' && g_ev_show_id) closeEpisodeView();
    if (e.key === 'Escape' && !document.getElementById('about-modal').classList.contains('hidden'))
        closeAboutModal();
    if (e.key === 'Escape' && !document.getElementById('manage-queues-modal').classList.contains('hidden'))
        closeManageQueuesModal();
});

loadQueues().then(() => { loadShows(); loadMovies(); });

// ---------- Queues -------------------------------------------------------

async function loadQueues() {
    const queues = await api('GET', '/api/queues');
    if (!Array.isArray(queues)) return;
    g_queues = queues;
    if (!queues.find(q => q.id === g_queue_id)) {
        g_queue_id = queues.length ? queues[0].id : 1;
        localStorage.setItem('fi_queue_id', g_queue_id);
    }
    renderQueueTabs();
}

function renderQueueTabs() {
    const container = document.getElementById('queue-tabs');
    if (!g_queues.length || g_queues.length === 1) {
        container.innerHTML = '';
        return;
    }
    container.innerHTML = g_queues.map(q =>
        `<button class="queue-tab-btn${q.id === g_queue_id ? ' active' : ''}"
                 onclick="selectQueue(${q.id})">${esc(q.name)}</button>`
    ).join('');
}

function moveToQueueHtml(type, id) {
    if (g_queues.length < 2) return '';
    const others = g_queues.filter(q => q.id !== g_queue_id);
    const opts = others.map(q =>
        `<option value="${q.id}">${esc(q.name)}</option>`).join('');
    return `<select class="btn-sm move-queue-select"
                    onchange="moveToQueue('${type}',${id},+this.value);this.selectedIndex=0">
              <option value="" selected disabled>Move to…</option>${opts}
            </select>`;
}

async function moveToQueue(type, id, queueId) {
    await api('PUT', `/api/${type}/${id}`, { queue_id: queueId });
    loadShows();
    loadMovies();
}

function selectQueue(queueId) {
    g_queue_id = queueId;
    localStorage.setItem('fi_queue_id', g_queue_id);
    renderQueueTabs();
    loadShows();
    loadMovies();
}

function openManageQueuesModal() {
    document.getElementById('menu-dropdown').classList.add('hidden');
    document.getElementById('manage-queues-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    renderManageQueues();
}

function closeManageQueuesModal() {
    document.getElementById('manage-queues-modal').classList.add('hidden');
    document.body.style.overflow = '';
}

function manageQueuesModalClick(event) {
    if (event.target === document.getElementById('manage-queues-modal'))
        closeManageQueuesModal();
}

function renderManageQueues() {
    const body = document.getElementById('manage-queues-body');
    const rows = g_queues.map(q => {
        const isOnly = g_queues.length <= 1;
        return `
        <div class="queue-row" data-id="${q.id}">
          <input type="text" class="queue-name-input" value="${esc(q.name)}"
                 onchange="renameQueue(${q.id}, this.value)">
          <button class="btn-sm" onclick="assignPinPrompt(${q.id})"
                  title="${q.pin ? 'Change PIN' : 'Assign PIN'}">PIN</button>
          ${isOnly ? '' : `<button class="btn-danger" onclick="deleteQueue(${q.id})">Delete</button>`}
        </div>`;
    }).join('');
    body.innerHTML = `
      ${rows}
      <div class="queue-add-row">
        <input type="text" id="new-queue-name" placeholder="New queue name..."
               autocomplete="off"
               onkeydown="if(event.key==='Enter'){event.preventDefault();addQueue();}">
        <button class="btn-add" onclick="addQueue()">+ Queue</button>
      </div>`;
}

async function addQueue() {
    const el = document.getElementById('new-queue-name');
    const name = el.value.trim();
    if (!name) return;
    await api('POST', '/api/queues', { name });
    el.value = '';
    await loadQueues();
    renderManageQueues();
}

async function renameQueue(id, newName) {
    newName = newName.trim();
    if (!newName) { renderManageQueues(); return; }
    await api('PUT', `/api/queues/${id}`, { name: newName });
    await loadQueues();
}

async function deleteQueue(id) {
    if (!confirm('Delete this queue? Its shows and movies will move to the default queue.')) return;
    await api('DELETE', `/api/queues/${id}`);
    if (g_queue_id === id) {
        g_queue_id = g_queues.length ? g_queues[0].id : 1;
        localStorage.setItem('fi_queue_id', g_queue_id);
    }
    await loadQueues();
    renderManageQueues();
    loadShows();
    loadMovies();
}

function assignPinPrompt(id) {
    const pin = prompt('Enter PIN for this queue (leave blank to remove):');
    if (pin === null) return;
    api('PUT', `/api/queues/${id}`, { pin: pin.trim() });
}

// ---------- About modal ---------------------------------------------------

function openAboutModal() {
    document.getElementById('menu-dropdown').classList.add('hidden');
    document.getElementById('about-modal').classList.remove('hidden');
    const body = document.getElementById('about-modal-body');
    body.innerHTML = '<div class="empty">Loading…</div>';
    fetch('/api/about')
        .then(r => r.json())
        .then(d => {
            body.innerHTML = `
              <div class="about-logo-row">
                <img src="images/FlickImp_icon.png" class="about-icon" alt="FlickImp">
              </div>
              <div class="about-name">${esc(d.name)}</div>
              <div class="about-version">Version ${esc(d.version)}</div>
              <div class="about-copyright">${esc(d.copyright)}</div>
              <div class="about-license">${esc(d.license)}</div>
              <div class="about-repo"><a href="${esc(d.repo)}" target="_blank" rel="noopener">${esc(d.repo)}</a></div>
              <div class="about-nutball-row">
                <img src="images/Nutball-Labs_logo.jpg" class="about-nutball-logo" alt="Nutball Labs">
              </div>`;
        })
        .catch(() => { body.innerHTML = '<div class="empty">Failed to load.</div>'; });
}

function closeAboutModal() {
    document.getElementById('about-modal').classList.add('hidden');
}

function aboutModalOverlayClick(e) {
    if (e.target === document.getElementById('about-modal')) closeAboutModal();
}

// ===== Release Calendar Module =====
let g_cal_date = new Date();
let g_cal_cache = null;

function navigateCalendar(deltaMonths) {
    g_cal_date.setMonth(g_cal_date.getMonth() + deltaMonths);
    renderCalendarGrid();
}

function resetCalendarToday() {
    g_cal_date = new Date();
    renderCalendarGrid();
}

async function loadCalendar(force = false) {
    const grid = document.getElementById('calendar-grid');
    if (!grid) return;
    if (g_cal_cache && !force) {
        renderCalendarGrid();
        return;
    }
    grid.innerHTML = '<div class="empty">Loading release schedules…</div>';

    try {
        const [shows, movies] = await Promise.all([
            api('GET', `/api/shows?queue_id=${g_queue_id}`),
            api('GET', `/api/movies?queue_id=${g_queue_id}`)
        ]);

        // Option 1: Filter to active shows (current)
        const currentShows = (Array.isArray(shows) ? shows : []).filter(s => s.queue === 'current');

        // Fetch episodes for current seasons concurrently
        const showEpisodePromises = currentShows.map(async s => {
            try {
                const targetSeason = s.next_season || s.season || 1;
                const episodes = await api('GET', `/api/shows/${s.id}/episodes?season=${targetSeason}`);
                if (!Array.isArray(episodes)) return [];
                return episodes
                    .filter(ep => ep.air_date)
                    .map(ep => ({
                        type: 'show',
                        id: s.id,
                        title: s.title,
                        subtitle: `S${ep.season}E${String(ep.episode).padStart(2, '0')}${ep.title ? ' - ' + ep.title : ''}`,
                        date: ep.air_date,
                        watched: ep.watched
                    }));
            } catch (err) {
                return [];
            }
        });

        const showResults = await Promise.all(showEpisodePromises);
        const allShowEvents = showResults.flat();

        const movieEvents = (Array.isArray(movies) ? movies : [])
            .filter(m => m.release_date && m.status !== 'watched')
            .map(m => ({
                type: 'movie',
                id: m.id,
                title: m.title,
                subtitle: 'Movie Release',
                date: m.release_date,
                watched: false
            }));

        g_cal_cache = [...allShowEvents, ...movieEvents];
        renderCalendarGrid();
    } catch (err) {
        console.error('Failed to load calendar data', err);
        grid.innerHTML = '<div class="empty">Error loading calendar schedules.</div>';
    }
}

function renderCalendarGrid() {
    const grid = document.getElementById('calendar-grid');
    const label = document.getElementById('calendar-month-label');
    if (!grid) return;

    const year = g_cal_date.getFullYear();
    const month = g_cal_date.getMonth();

    const monthNames = [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ];
    if (label) label.textContent = `${monthNames[month]} ${year}`;

    const firstDayIndex = new Date(year, month, 1).getDay();
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    const prevDaysInMonth = new Date(year, month, 0).getDate();

    const todayStr = new Date().toISOString().slice(0, 10);
    const dayHeaders = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

    let html = '<div class="calendar-header-row">';
    dayHeaders.forEach(day => {
        html += `<div class="calendar-day-header">${day}</div>`;
    });
    html += '</div><div class="calendar-days-grid">';

    // Leading days from previous month
    for (let i = firstDayIndex - 1; i >= 0; i--) {
        const d = prevDaysInMonth - i;
        html += `<div class="calendar-day other-month"><span class="day-number">${d}</span></div>`;
    }

    // Days in current month
    for (let day = 1; day <= daysInMonth; day++) {
        const dateStr = `${year}-${String(month + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
        const isToday = dateStr === todayStr;
        const events = (g_cal_cache || []).filter(e => e.date === dateStr);

        html += `
        <div class="calendar-day${isToday ? ' today' : ''}">
            <div class="day-top"><span class="day-number">${day}</span></div>
            <div class="day-events">
                ${events.map(ev => `
                    <div class="cal-event ${ev.type}${ev.watched ? ' watched' : ''}" 
                         title="${esc(ev.title)}:${esc(ev.subtitle)}"
                         onclick="${ev.type === 'show' ? `openEpisodeView(${ev.id}, '${esc(ev.title).replace(/'/g, "\\'")}')` : `switchMainTab('movies')`}">
                        <span class="event-tag">${ev.type === 'movie' ? '🎬' : '📺'}</span>
                        <div class="event-details">
                            <span class="event-title">${esc(ev.title)}</span>
                            <span class="event-sub">${esc(ev.subtitle)}</span>
                        </div>
                    </div>
                `).join('')}
            </div>
        </div>`;
    }

    // Trailing days into next month to complete row of 7
    const totalCells = firstDayIndex + daysInMonth;
    const trailingDays = (7 - (totalCells % 7)) % 7;
    for (let day = 1; day <= trailingDays; day++) {
        html += `<div class="calendar-day other-month"><span class="day-number">${day}</span></div>`;
    }

    html += '</div>';
    grid.innerHTML = html;
}
// SN: 00004
