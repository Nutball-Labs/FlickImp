'use strict';

// Queue PIN unlock state (see "Queue PINs" below). Declared first because
// api() reads it, and the first api() call runs during script start-up.
let g_queue_tokens  = {};     // queue id -> unlock token
let g_lock_recovery = null;   // shared in-flight recovery (shows + movies both hit it)

// ---------- API helpers ---------------------------------------------------

async function api(method, path, body) {
    const opts = { method, headers: { 'Content-Type': 'application/json' } };
    const tokens = Object.values(g_queue_tokens).filter(Boolean);
    if (tokens.length) opts.headers['X-Queue-Tokens'] = tokens.join(',');
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
let g_new_show_first_air = '';   // first_air_date of the chosen TMDB result
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
    const airArg   = JSON.stringify(r.first_air_date || '').replace(/"/g, '&quot;');
    return `
    <div class="search-result-item"
         onclick="${selectFn}(${r.tmdb_id}, ${titleArg}, ${thumbArg}, ${airArg})">
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

function selectShowResult(tmdbId, title, posterUrl, firstAirDate) {
    g_new_show_tmdb_id   = tmdbId;
    g_new_show_thumb     = posterUrl;
    g_new_show_first_air = firstAirDate || '';
    document.getElementById('new-show-title').value = title;
    document.getElementById('show-search-results').classList.add('hidden');
    updateCaughtUpRow();
}

// "Mark all aired episodes watched" is offered once the show has started
// airing. With a TMDB match we know the first air date; with only a typed
// IMDB ID we don't, so offer it and let the server check.
function updateCaughtUpRow() {
    const row = document.getElementById('new-show-caughtup-row');
    const imdb = document.getElementById('new-show-imdb').value.trim();
    const aired = g_new_show_tmdb_id
        ? (g_new_show_first_air !== '' && g_new_show_first_air <= localDateStr(new Date()))
        : imdb !== '';
    row.classList.toggle('hidden', !aired);
    if (!aired) {
        document.getElementById('new-show-caughtup').checked = false;
        caughtUpToggled();
    }
}

function caughtUpToggled() {
    const on = document.getElementById('new-show-caughtup').checked;
    const ep = document.getElementById('new-show-ep');
    ep.disabled = on;
    ep.placeholder = on ? 'set from TMDB' : 'leave blank = not started';
}

// One TMDB call server-side; writes watched rows for every aired episode.
// Returns true on success, otherwise alerts the reason.
async function markAiredWatched(showId) {
    const r = await api('POST', `/api/shows/${showId}/mark-aired-watched`);
    if (r.error) { alert('Could not mark episodes watched: ' + r.error); return false; }
    return true;
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
    reorderShowEntries(g_drag_id, targetId, insertAfter);
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
        reorderShowEntries(g_drag_id, atStart ? 'start' : 'end');
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

let g_main_tab  = 'shows';   // 'shows' | 'movies' | 'calendar'
let g_shows_tab = 'current'; // 'current' | 'queued'
let g_shows     = [];        // last loaded shows, for sub-tab re-render
let g_movies    = [];        // last loaded movies, for drag reorder
let g_queues    = [];
// Start-up queue: a fixed choice from Settings, else the last one used
let g_queue_id  = parseInt(localStorage.getItem('fi_startup_queue'))
               || parseInt(localStorage.getItem('fi_queue_id')) || 1;
let g_cal_date  = new Date(); // month shown in the release calendar
let g_cal_cache = null;       // calendar events; null = stale, refetch on next view

function switchMainTab(tab) {
    g_main_tab = tab;
    localStorage.setItem('fi_last_tab', tab);
    document.getElementById('tab-shows').classList.toggle('active', tab === 'shows');
    document.getElementById('tab-movies').classList.toggle('active', tab === 'movies');
    const calTab = document.getElementById('tab-calendar');
    if (calTab) calTab.classList.toggle('active', tab === 'calendar');

    document.getElementById('shows-section').classList.toggle('hidden', tab !== 'shows');
    document.getElementById('movies-section').classList.toggle('hidden', tab !== 'movies');
    const calSec = document.getElementById('calendar-section');
    if (calSec) calSec.classList.toggle('hidden', tab !== 'calendar');

    if (tab === 'calendar') {
        loadCalendar();
    }
}

function switchShowsTab(tab) {
    g_shows_tab = tab;
    document.getElementById('subtab-current').classList.toggle('active', tab === 'current');
    document.getElementById('subtab-queued').classList.toggle('active', tab === 'queued');
    renderShows(g_shows);
}

async function loadShows() {
    g_cal_cache = null;
    const [shows, groups] = await Promise.all([
        api('GET', `/api/shows?queue_id=${g_queue_id}`),
        api('GET', '/api/groups'),
    ]);
    if (shows && shows.locked) {
        if (await handleLockedQueue()) return loadShows();
        document.getElementById('shows-list').innerHTML = lockedListHtml();
        return;
    }
    g_show_groups = Array.isArray(groups) ? groups : [];
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
          ${groupWithHtml(s)}
          ${(s.tmdb_id || s.imdb_id) ? `
          <label class="check-row span2">
            <input type="checkbox" id="ecaughtup-${s.id}"
                   onchange="document.getElementById('eep-${s.id}').disabled = this.checked">
            Mark all aired episodes as watched
          </label>` : ''}
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

function renderShows(shows) {
    const list = document.getElementById('shows-list');
    const entries = showEntries(shows);
    if (!entries.length) {
        list.innerHTML = g_shows_tab === 'queued'
            ? '<div class="empty">No queued shows.</div>'
            : '<div class="empty">No shows yet — add one above.</div>';
        return;
    }
    list.innerHTML = entries.map(e => e.show ? showCard(e.show) : groupCard(e)).join('');
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
    const caughtUpEl = document.getElementById(`ecaughtup-${id}`);
    const caughtUp   = caughtUpEl && caughtUpEl.checked;
    if (pos && !caughtUp) { body.season = pos.season; body.episode = pos.episode; }

    await api('PUT', `/api/shows/${id}`, body);
    if (caughtUp) await markAiredWatched(id);
    await applyGroupChoice(id);
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
    g_new_show_tmdb_id   = 0;
    g_new_show_thumb     = '';
    g_new_show_first_air = '';
    updateCaughtUpRow();
}

async function addShow() {
    const title = document.getElementById('new-show-title').value.trim();
    if (!title) { alert('Title is required.'); return; }

    const caughtUp = document.getElementById('new-show-caughtup').checked;
    const raw = (!caughtUp && document.getElementById('new-show-ep').value.trim()) || 's000-e000';
    const pos  = parseEp(raw);
    if (!pos) { alert('Use the format s001-e001'); return; }

    const added = await api('POST', '/api/shows', {
        title,
        service:       document.getElementById('new-show-service').value.trim(),
        season:        pos.season,
        episode:       pos.episode,
        imdb_id:       document.getElementById('new-show-imdb').value.trim(),
        tmdb_id:       g_new_show_tmdb_id,
        thumbnail_url: g_new_show_thumb,
        queue_id:      g_queue_id,
    });
    if (added.error) { alert('Could not add show: ' + added.error); return; }
    hideAddShowForm();
    if (caughtUp) await markAiredWatched(added.id);
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
              onclick="${clickFn}(${showId}, ${s.season})"
              oncontextmenu="seasonContextMenu(event,${showId},${s.season},${s.watched_count},${s.total_episodes})">
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
    g_cal_cache = null;

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
    g_cal_cache = null;
    const movies = await api('GET', `/api/movies?queue_id=${g_queue_id}`);
    if (movies && movies.locked) {
        if (await handleLockedQueue()) return loadMovies();
        document.getElementById('movies-list').innerHTML = lockedListHtml();
        return;
    }
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
    if (g_ev_show_id || g_gv_group) closeEpisodeView();
}

async function openEpisodeView(showId, title) {
    g_ev_show_id = showId;
    document.getElementById('ev-title').textContent = title;
    document.getElementById('ev-seasons-pane').innerHTML = '<div class="empty">Loading…</div>';
    document.getElementById('ev-episodes-pane').innerHTML = '<div class="empty">Select a season</div>';
    document.getElementById('main-view').classList.add('hidden');
    document.getElementById('episode-view').classList.remove('hidden');
    document.getElementById('site-title').classList.add('title-nav');
    document.body.classList.add('ev-open');
    window.scrollTo(0, 0);

    const seasons = await api('GET', `/api/shows/${showId}/seasons`);
    if (!seasons.error)
        renderSeasons(showId, seasons, 0, 'ev-seasons-pane', 'selectEpisodeViewSeason');
}

function closeEpisodeView() {
    document.getElementById('episode-view').classList.add('hidden');
    document.getElementById('main-view').classList.remove('hidden');
    document.getElementById('site-title').classList.remove('title-nav');
    document.body.classList.remove('ev-open');
    g_ev_show_id = 0;
    g_ev_season  = 0;
    g_gv_group   = null;
    g_gv_sel     = null;
    document.getElementById('ev-mode-toggle').classList.add('hidden');
    loadShows();  // refresh card in case watched state changed
}

async function selectEpisodeViewSeason(showId, season) {
    g_ev_season = season;
    document.querySelectorAll('#ev-seasons-pane .season-btn').forEach(b => b.classList.remove('season-active'));
    const btn = document.getElementById(`sbtn-${showId}-${season}`);
    if (btn) btn.classList.add('season-active');

    const pane = document.getElementById('ev-episodes-pane');
    pane.innerHTML = '<div class="empty">Loading episodes…</div>';
    pane.scrollTop = 0;
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
    pane.innerHTML = evAllRowHtml(showId, season) + episodes.map(ep => {
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
              <input type="checkbox" ${ep.watched ? 'checked' : ''} data-aired="${isAired(ep) ? 1 : 0}"
                     onchange="evToggleWatched(${showId},${season},${ep.episode},this.checked,${seasonTotal})">
              <span>Watched</span>
            </label>
            <button class="btn-sm" onclick="openEpisodeCast(${showId},${season},${ep.episode},${epTitleJson})">Cast</button>
          </div>
        </div>`;
    }).join('');
    syncEvAllBox();
}

async function evToggleWatched(showId, season, episode, watched, seasonTotal) {
    await api('PUT', `/api/shows/${showId}/episodes/${season}/${episode}/watched`,
              {watched, season_total: seasonTotal});
    g_cal_cache = null;
    const card = document.getElementById(`epcard-${showId}-${season}-${episode}`);
    if (card) card.classList.toggle('ep-card-watched', watched);
    syncEvAllBox();
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
    const menu = document.getElementById('season-ctx-menu');
    if (!menu.classList.contains('hidden') && !menu.contains(e.target))
        closeSeasonMenu();
});
window.addEventListener('scroll', closeSeasonMenu, true);

document.addEventListener('keydown', e => {
    if (e.key === 'Escape') { closeEpLinkPopup(); closeSeasonMenu(); }
    if (e.key === 'Escape' && g_popup_show_id) closeWatchedPopup();
    if (e.key === 'Escape' && !document.getElementById('check-modal').classList.contains('hidden'))
        closeCheckModal();
    if (e.key === 'Escape' && !document.getElementById('cast-modal').classList.contains('hidden'))
        closeCastModal();
    if (e.key === 'Escape' && (g_ev_show_id || g_gv_group)) closeEpisodeView();
    if (e.key === 'Escape' && !document.getElementById('about-modal').classList.contains('hidden'))
        closeAboutModal();
    if (e.key === 'Escape' && !document.getElementById('manage-queues-modal').classList.contains('hidden'))
        closeManageQueuesModal();
    if (e.key === 'Escape' && !document.getElementById('settings-modal').classList.contains('hidden'))
        closeSettingsModal();
    if (e.key === 'Escape' && !document.getElementById('backup-modal').classList.contains('hidden'))
        closeBackupModal();
});

loadQueues().then(() => {
    loadShows();
    loadMovies();
    // Start-up tab: a fixed choice from Settings, else the last one used
    const pref = localStorage.getItem('fi_startup_tab') || 'last';
    const tab  = pref === 'last' ? localStorage.getItem('fi_last_tab') : pref;
    if (['movies', 'calendar'].includes(tab)) switchMainTab(tab);
});

// ---------- Queue PINs ------------------------------------------------------
// Speed bump, not security: the server hides a PIN-protected queue's items
// unless the request carries an unlock token for it (X-Queue-Tokens header,
// added by api()). Tokens live only in this page, so a reload re-locks, and
// a daemon restart forgets them (handled in handleLockedQueue).


// Masked PIN entry. Resolves to the entered string, or null on Cancel/Escape.
function askPin({ title, message = '', error = '', allowEmpty = false }) {
    return new Promise(resolve => {
        const modal  = document.getElementById('pin-modal');
        const input  = document.getElementById('pin-modal-input');
        const errEl  = document.getElementById('pin-modal-error');
        document.getElementById('pin-modal-title').textContent = title;
        document.getElementById('pin-modal-msg').textContent   = message;
        errEl.textContent = error;
        input.value = '';
        modal.classList.remove('hidden');
        setTimeout(() => input.focus(), 0);

        const done = value => {
            modal.classList.add('hidden');
            input.onkeydown = null;
            resolve(value);
        };
        const submit = () => {
            const v = input.value.trim();
            if (!v && !allowEmpty) { errEl.textContent = 'Enter a PIN.'; return; }
            done(v);
        };
        document.getElementById('pin-ok').onclick     = submit;
        document.getElementById('pin-cancel').onclick = () => done(null);
        input.onkeydown = e => {
            if (e.key === 'Enter')  { e.preventDefault(); submit(); }
            if (e.key === 'Escape') { e.stopPropagation(); done(null); }
        };
    });
}

// True if the queue can be viewed now (no PIN, already unlocked, or the
// user just entered the right PIN); false if they cancelled.
async function ensureUnlocked(queueId) {
    const q = g_queues.find(x => x.id === queueId);
    if (!q || !q.has_pin || g_queue_tokens[queueId]) return true;
    let error = '';
    for (;;) {
        const pin = await askPin({ title: `🔒 ${q.name}`, message: 'Enter the PIN for this queue.', error });
        if (pin === null) return false;
        const r = await api('POST', `/api/queues/${queueId}/unlock`, { pin });
        if (r.token) {
            g_queue_tokens[queueId] = r.token;
            renderQueueTabs();
            return true;
        }
        error = r.error || 'Wrong PIN';
    }
}

// The server said the current queue is locked (fresh page, or the daemon
// restarted and forgot our token). Ask for the PIN; on Cancel move to the
// first queue without one. Returns true if a reload will now succeed.
function handleLockedQueue() {
    if (!g_lock_recovery) {
        g_lock_recovery = (async () => {
            delete g_queue_tokens[g_queue_id];
            if (await ensureUnlocked(g_queue_id)) return true;
            const open = g_queues.find(q => !q.has_pin);
            if (!open) return false;
            g_queue_id = open.id;
            localStorage.setItem('fi_queue_id', g_queue_id);
            renderQueueTabs();
            return true;
        })().finally(() => { g_lock_recovery = null; });
    }
    return g_lock_recovery;
}

function lockedListHtml() {
    return '<div class="empty">🔒 This queue is locked.</div>';
}

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
    const lock = q => !q.has_pin ? '' : (g_queue_tokens[q.id] ? ' 🔓' : ' 🔒');
    container.innerHTML = g_queues.map(q =>
        `<button class="queue-tab-btn${q.id === g_queue_id ? ' active' : ''}"
                 onclick="selectQueue(${q.id})">${esc(q.name)}${lock(q)}</button>`
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
    if (type === 'group')
        await Promise.all(groupMembers(id).map(s => api('PUT', `/api/shows/${s.id}`, { queue_id: queueId })));
    else
        await api('PUT', `/api/${type}/${id}`, { queue_id: queueId });
    loadShows();
    loadMovies();
}

async function selectQueue(queueId) {
    if (!(await ensureUnlocked(queueId))) return;
    g_queue_id = queueId;
    localStorage.setItem('fi_queue_id', g_queue_id);
    renderQueueTabs();
    loadShows();
    loadMovies();
    if (g_main_tab === 'calendar') loadCalendar();
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
          <button class="btn-sm pin-btn ${q.has_pin ? 'pin-set' : 'pin-unset'}"
                  onclick="assignPinPrompt(${q.id})"
                  title="${q.has_pin ? 'PIN set — click to change or remove' : 'No PIN — click to assign one'}">PIN</button>
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
    if (!(await ensureUnlocked(id))) return;
    if (!confirm('Delete this queue? Its shows and movies will move to the default queue.')) return;
    const r = await api('DELETE', `/api/queues/${id}`);
    if (r.error) { alert(r.error); return; }
    delete g_queue_tokens[id];
    if (g_queue_id === id) {
        g_queue_id = g_queues.length ? g_queues[0].id : 1;
        localStorage.setItem('fi_queue_id', g_queue_id);
    }
    await loadQueues();
    renderManageQueues();
    loadShows();
    loadMovies();
}

async function assignPinPrompt(id) {
    const q = g_queues.find(x => x.id === id);
    if (!q) return;
    if (q.has_pin && !(await ensureUnlocked(id))) return;   // must know the current PIN
    const pin = await askPin({
        title:      q.has_pin ? `Change PIN — ${q.name}` : `Set a PIN — ${q.name}`,
        message:    q.has_pin ? 'Enter a new PIN, or leave it blank to remove the PIN.'
                              : 'Anyone opening this queue will need this PIN.',
        allowEmpty: q.has_pin,
    });
    if (pin === null) return;
    const r = await api('PUT', `/api/queues/${id}`, { pin });
    if (r.error) { alert(r.error); return; }
    if (pin) {
        // Stay unlocked in this browser with the new PIN
        const u = await api('POST', `/api/queues/${id}/unlock`, { pin });
        if (u.token) g_queue_tokens[id] = u.token;
    } else {
        delete g_queue_tokens[id];
    }
    await loadQueues();      // refresh has_pin so the button colour updates
    renderManageQueues();
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

// ---------- Show groups ----------------------------------------------------
// Several shows (Doctor Who 1963 / 2005 / 2023, the Jeopardy family) shown as
// one card. Members keep their own progress; the group's Last/Next come from
// the server, worked out across members in air-date order. The group's place
// in the list is its members' place, so reordering still sends show ids.

let g_show_groups     = [];          // GET /api/groups
let g_open_group_edit = new Set();   // group ids whose edit panel is open

// Member's distinguishing part: "Doctor Who (2005)" in group "Doctor Who" -> "(2005)"
function memberLabel(groupName, title) {
    const g = groupName.toLowerCase(), t = (title || '').toLowerCase();
    if (t.startsWith(g)) {
        const rest = title.slice(groupName.length).replace(/^[\s:\-–—]+/, '').trim();
        if (rest) return rest;
    }
    return title;
}

// List entries for the current Current/Queued tab: plain shows and groups
function showEntries(shows) {
    const isQueued = g_shows_tab === 'queued';
    const entries = [];
    const groups  = new Map();
    for (const s of shows.filter(s => (s.queue === 'queued') === isQueued)) {
        const g = s.group_id && g_show_groups.find(x => x.id === s.group_id);
        if (!g) { entries.push({ key: s.id, show: s, title: s.title }); continue; }
        let e = groups.get(g.id);
        if (!e) {
            e = { key: 'g' + g.id, group: g, members: [], title: g.name };
            groups.set(g.id, e);
            entries.push(e);
        }
        e.members.push(s);
    }
    for (const e of groups.values())
        e.members.sort((a, b) => a.group_order - b.group_order);

    const sortOrder = e => e.show ? (e.show.sort_order || 999999)
                                  : Math.min(...e.members.map(m => m.sort_order || 999999));
    const rank = e => {
        const ms = e.show ? [e.show] : e.members;
        if (ms.every(s => s.status === 'finished')) return 2;
        if (ms.some(showHasNew)) return 0;
        return 1;
    };
    entries.sort((a, b) => {
        const d = sortOrder(a) - sortOrder(b);
        if (d) return d;
        const r = rank(a) - rank(b);
        return r || a.title.localeCompare(b.title);
    });
    return entries;
}

function entryIds(e) {
    return e.show ? [e.show.id] : e.members.map(m => m.id);
}

// Move one list entry (show id or 'g<id>') before/after another, or to
// 'start' / 'end', then save the flattened show order.
function reorderShowEntries(dragKey, targetKey, after) {
    const entries = showEntries(g_shows);
    const from = entries.findIndex(e => e.key === dragKey);
    if (from < 0) return;
    const [moved] = entries.splice(from, 1);
    let to;
    if (targetKey === 'start')    to = 0;
    else if (targetKey === 'end') to = entries.length;
    else {
        to = entries.findIndex(e => e.key === targetKey);
        if (to < 0) return;
        if (after) to += 1;
    }
    entries.splice(to, 0, moved);
    const ids = entries.flatMap(entryIds);
    ids.forEach((id, i) => {
        const show = g_shows.find(s => s.id === id);
        if (show) show.sort_order = i + 1;
    });
    renderShows(g_shows);
    api('PUT', '/api/shows/reorder', { order: ids });
}

function groupEpLabel(groupName, ep) {
    return `${memberLabel(groupName, ep.show_title)} S${ep.season} &minus; E${ep.episode}`;
}

function groupCard(e) {
    const g = e.group, members = e.members;
    const gid = g.id;
    const nextMember = g.next && members.find(s => s.id === g.next.show_id);
    const thumbSrc = (nextMember && nextMember.thumbnail_url)
        || (members.find(s => s.thumbnail_url) || {}).thumbnail_url || '';
    const thumbHtml = thumbSrc ? `<img class="card-thumb" src="${esc(thumbSrc)}" alt="" loading="lazy">` : '';
    const hasNew   = members.some(showHasNew);
    const allDone  = members.every(s => s.status === 'finished');
    const queue    = members[0].queue;

    const lastHtml = g.last
        ? `Last watched: ${groupEpLabel(g.name, g.last)}`
        : 'Last watched: <em>Not started</em>';
    let nextHtml = '';
    if (g.next) {
        const today = localDateStr(new Date());
        const airs  = g.next.air_date && g.next.air_date > today ? ` (airs ${esc(g.next.air_date)})` : '';
        nextHtml = `<div class="${hasNew ? 'ep-next-watch ep-next-watch-new' : 'ep-next-watch'}">
              Next: ${groupEpLabel(g.name, g.next)}${g.next.title ? ' — ' + esc(g.next.title) : ''}${airs}
            </div>`;
    }

    const editOpen = g_open_group_edit.has(gid);
    const memberRows = members.map((s, i) => `
          <div class="group-member-row">
            <span class="group-member-title">${esc(s.title)}</span>
            <button class="btn-sm" title="Move up" ${i === 0 ? 'disabled' : ''}
                    onclick="moveGroupMember(${gid},${s.id},-1)">&uarr;</button>
            <button class="btn-sm" title="Move down" ${i === members.length - 1 ? 'disabled' : ''}
                    onclick="moveGroupMember(${gid},${s.id},1)">&darr;</button>
            <button class="btn-danger" onclick="removeGroupMember(${gid},${s.id})">Remove</button>
          </div>`).join('');

    return `
    <div class="show-wrap group-wrap${editOpen ? ' panel-open' : ''}" id="gwrap-${gid}"
         draggable="true"
         ondragstart="itemDragStart(event,'show','g${gid}')"
         ondragend="itemDragEnd(event)"
         ondragover="itemDragOver(event)"
         ondragleave="itemDragLeave(event)"
         ondrop="showDrop(event,'g${gid}')">

      <div class="card group-card">
        <div class="drag-handle" onmousedown="dragHandleDown(event)"
             ontouchstart="dragHandleDown(event)" title="Drag to reorder">&#x2630;</div>
        ${thumbHtml}
        <div class="card-body">
          <div class="card-top">
            <div class="card-title">
              <a class="show-title-link" href="#"
                 onclick="event.preventDefault();openGroupView(${gid})">${esc(g.name)}</a>
            </div>
            <span class="badge badge-group" title="${esc(members.map(s => s.title).join(' · '))}">${members.length} series</span>
            ${allDone ? '<span class="badge badge-finished">Finished</span>' : ''}
            ${hasNew ? '<span class="badge badge-new">NEW</span>' : ''}
          </div>
          <div class="ep-track">
            <div class="ep-last-watched" onclick="openGroupView(${gid})">${lastHtml}</div>
            ${nextHtml}
          </div>
          <div class="card-actions">
            <button class="btn-sm" onclick="toggleGroupEdit(${gid})">Edit</button>
            <button class="btn-sm" onclick="toggleGroupQueue(${gid},'${queue}')">
              ${queue === 'queued' ? '→ Current' : '→ Queued'}
            </button>
            ${moveToQueueHtml('group', gid)}
            <button class="btn-danger" onclick="ungroup(${gid})">Ungroup</button>
          </div>
        </div>
      </div>

      <div class="edit-panel${editOpen ? '' : ' hidden'}" id="gedit-${gid}">
        <div class="form-grid">
          <div class="field span2">
            <label for="gname-${gid}">Group name</label>
            <input type="text" id="gname-${gid}" value="${esc(g.name)}" autocomplete="off">
          </div>
          <div class="field span2">
            <label>Shows in this group</label>
            ${memberRows}
          </div>
        </div>
        <div class="form-actions">
          <button class="btn-primary" onclick="saveGroupName(${gid})">Save</button>
          <button class="btn-cancel"  onclick="toggleGroupEdit(${gid})">Close</button>
        </div>
      </div>
    </div>`;
}

function groupMembers(gid) {
    return g_shows.filter(s => s.group_id === gid).sort((a, b) => a.group_order - b.group_order);
}

function toggleGroupEdit(gid) {
    if (g_open_group_edit.has(gid)) g_open_group_edit.delete(gid);
    else g_open_group_edit.add(gid);
    renderShows(g_shows);
}

async function saveGroupName(gid) {
    const name = document.getElementById(`gname-${gid}`).value.trim();
    if (!name) { alert('Group name is required.'); return; }
    await api('PUT', `/api/groups/${gid}`, { name });
    g_open_group_edit.delete(gid);
    loadShows();
}

async function moveGroupMember(gid, showId, dir) {
    const ids = groupMembers(gid).map(s => s.id);
    const i = ids.indexOf(showId), j = i + dir;
    if (i < 0 || j < 0 || j >= ids.length) return;
    [ids[i], ids[j]] = [ids[j], ids[i]];
    await api('PUT', `/api/groups/${gid}`, { order: ids });
    loadShows();
}

async function removeGroupMember(gid, showId) {
    const left = groupMembers(gid).length - 1;
    const msg = left < 2
        ? 'Remove this show from the group? The group only has two shows, so it will be dissolved.'
        : 'Remove this show from the group? It stays in your list as its own show.';
    if (!confirm(msg)) return;
    await api('DELETE', `/api/groups/${gid}/members/${showId}`);
    if (left < 2) g_open_group_edit.delete(gid);
    loadShows();
}

async function ungroup(gid) {
    if (!confirm('Ungroup these shows? Each goes back to being its own card; nothing is deleted.')) return;
    await api('DELETE', `/api/groups/${gid}`);
    g_open_group_edit.delete(gid);
    loadShows();
}

// Current/Queued and named-queue moves apply to every member
async function toggleGroupQueue(gid, current) {
    const next = current === 'queued' ? 'current' : 'queued';
    await Promise.all(groupMembers(gid).map(s => api('PUT', `/api/shows/${s.id}`, { queue: next })));
    loadShows();
}

// "Group with…" picker in a plain show's Edit panel
function groupWithHtml(s) {
    if (s.group_id) return '';
    const sameList = x => x.queue === s.queue && x.queue_id === s.queue_id;
    const groupOpts = g_show_groups
        .filter(g => g_shows.some(x => x.group_id === g.id && sameList(x)))
        .map(g => `<option value="g:${g.id}">Add to group: ${esc(g.name)}</option>`).join('');
    const showOpts = g_shows
        .filter(x => x.id !== s.id && !x.group_id && sameList(x))
        .sort((a, b) => a.title.localeCompare(b.title))
        .map(x => `<option value="s:${x.id}">Group with: ${esc(x.title)}</option>`).join('');
    if (!groupOpts && !showOpts) return '';
    return `
          <div class="field span2">
            <label for="egroup-${s.id}">Show group</label>
            <select id="egroup-${s.id}">
              <option value="">Not grouped</option>${groupOpts}${showOpts}
            </select>
          </div>`;
}

async function applyGroupChoice(showId) {
    const el = document.getElementById(`egroup-${showId}`);
    if (!el || !el.value) return;
    const [kind, id] = el.value.split(':');
    const r = kind === 'g'
        ? await api('POST', `/api/groups/${id}/members`, { show_id: showId })
        : await api('POST', '/api/groups', { show_ids: [+id, showId] });
    if (r.error) alert('Could not group: ' + r.error);
}

// ---------- Group episode browser -----------------------------------------
// Same view as a single show's browser; the left pane lists either every
// member's seasons (premiere order) or years, chosen per group.

let g_gv_group = null;   // group being browsed
let g_gv_sel   = null;   // {show_id, season} or {year}

function groupViewMode(gid) {
    return localStorage.getItem(`fi_group_mode_${gid}`) || 'season';
}

async function openGroupView(gid) {
    const g = g_show_groups.find(x => x.id === gid);
    if (!g) return;
    g_gv_group = g;
    g_gv_sel   = null;
    document.getElementById('ev-title').textContent = g.name;
    document.getElementById('ev-episodes-pane').innerHTML = '<div class="empty">Select a season</div>';
    document.getElementById('main-view').classList.add('hidden');
    document.getElementById('episode-view').classList.remove('hidden');
    document.getElementById('site-title').classList.add('title-nav');
    document.body.classList.add('ev-open');
    window.scrollTo(0, 0);
    renderGroupModeToggle();
    await loadGroupPane();
}

function renderGroupModeToggle() {
    const el = document.getElementById('ev-mode-toggle');
    const mode = groupViewMode(g_gv_group.id);
    el.innerHTML = `
      <button class="tab-btn${mode === 'season' ? ' active' : ''}" onclick="setGroupViewMode('season')">By season</button>
      <button class="tab-btn${mode === 'year' ? ' active' : ''}" onclick="setGroupViewMode('year')">By year</button>`;
    el.classList.remove('hidden');
}

function setGroupViewMode(mode) {
    localStorage.setItem(`fi_group_mode_${g_gv_group.id}`, mode);
    g_gv_sel = null;
    renderGroupModeToggle();
    document.getElementById('ev-episodes-pane').innerHTML =
        `<div class="empty">Select a ${mode === 'year' ? 'year' : 'season'}</div>`;
    loadGroupPane();
}

// (Re)draw the left pane, keeping the current selection highlighted
async function loadGroupPane() {
    const g = g_gv_group;
    const pane = document.getElementById('ev-seasons-pane');
    const mode = groupViewMode(g.id);
    if (!pane.querySelector('.season-btn'))
        pane.innerHTML = mode === 'year'
            ? '<div class="empty">Loading every episode list… the first time can take a while for a long-running group.</div>'
            : '<div class="empty">Loading…</div>';

    if (mode === 'year') {
        const years = await api('GET', `/api/groups/${g.id}/years`);
        if (g_gv_group !== g || groupViewMode(g.id) !== 'year') return;
        if (!Array.isArray(years)) { pane.innerHTML = `<div class="empty">${esc(years.error || 'Failed to load')}</div>`; return; }
        if (!years.length) { pane.innerHTML = '<div class="empty">No episodes found.</div>'; return; }
        pane.innerHTML = years.map(y => `
          <button class="season-btn ${seasonClass(y.watched_count, y.total_episodes)}${g_gv_sel && g_gv_sel.year === y.year ? ' season-active' : ''}"
                  id="gybtn-${y.year}" onclick="selectGroupYear(${y.year})">
            ${y.year || 'TBA'}
            <span class="season-ep-count">${y.watched_count}/${y.total_episodes}</span>
          </button>`).join('');
    } else {
        const seasons = await api('GET', `/api/groups/${g.id}/seasons`);
        if (g_gv_group !== g || groupViewMode(g.id) !== 'season') return;
        if (!Array.isArray(seasons)) { pane.innerHTML = `<div class="empty">${esc(seasons.error || 'Failed to load')}</div>`; return; }
        if (!seasons.length) { pane.innerHTML = '<div class="empty">No seasons found.</div>'; return; }
        pane.innerHTML = seasons.map(s => {
            const active = g_gv_sel && g_gv_sel.show_id === s.show_id && g_gv_sel.season === s.season;
            return `
          <button class="season-btn ${seasonClass(s.watched_count, s.total_episodes)}${active ? ' season-active' : ''}"
                  id="gsbtn-${s.show_id}-${s.season}" onclick="selectGroupSeason(${s.show_id},${s.season})"
                  oncontextmenu="seasonContextMenu(event,${s.show_id},${s.season},${s.watched_count},${s.total_episodes})"
                  title="${esc(s.show_title)}${s.air_date ? ' — ' + esc(s.air_date) : ''}">
            <span class="season-btn-series">${esc(memberLabel(g.name, s.show_title))}</span>
            S${String(s.season).padStart(2, '0')}
            <span class="season-ep-count">${s.watched_count}/${s.total_episodes}</span>
          </button>`;
        }).join('');
    }
}

function markGroupPaneActive(btnId) {
    document.querySelectorAll('#ev-seasons-pane .season-btn').forEach(b => b.classList.remove('season-active'));
    const btn = document.getElementById(btnId);
    if (btn) btn.classList.add('season-active');
}

async function selectGroupSeason(showId, season) {
    g_gv_sel = { show_id: showId, season };
    markGroupPaneActive(`gsbtn-${showId}-${season}`);
    await loadGroupEpisodes(`show_id=${showId}&season=${season}`);
}

async function selectGroupYear(year) {
    g_gv_sel = { year };
    markGroupPaneActive(`gybtn-${year}`);
    await loadGroupEpisodes(`year=${year}`);
}

async function loadGroupEpisodes(query) {
    const pane = document.getElementById('ev-episodes-pane');
    pane.innerHTML = '<div class="empty">Loading episodes…</div>';
    pane.scrollTop = 0;
    const eps = await api('GET', `/api/groups/${g_gv_group.id}/episodes?${query}`);
    if (!Array.isArray(eps)) { pane.innerHTML = `<div class="empty">${esc(eps.error || 'Failed to load')}</div>`; return; }
    if (!eps.length) { pane.innerHTML = '<div class="empty">No episodes found.</div>'; return; }
    const groupName = g_gv_group.name;
    const seasonView = g_gv_sel && g_gv_sel.season;
    pane.innerHTML = (seasonView ? evAllRowHtml(g_gv_sel.show_id, g_gv_sel.season) : '') + eps.map(ep => {
        const epTitleJson = JSON.stringify(ep.title || '').replace(/"/g, '&quot;');
        const cardId = `gepcard-${ep.show_id}-${ep.season}-${ep.episode}`;
        return `
        <div class="ep-card${ep.watched ? ' ep-card-watched' : ''}" id="${cardId}">
          <div class="ep-card-body">
            <div class="ep-card-series">${esc(memberLabel(groupName, ep.show_title))}</div>
            <div class="ep-card-heading">
              <span class="ep-card-num">S${ep.season}E${String(ep.episode).padStart(3, '0')}</span>
              <span class="ep-card-title ep-card-title-link"
                    onclick="openGroupEpLink(event,${JSON.stringify(ep.episode_url).replace(/"/g, '&quot;')},${ep.show_id},${ep.season},${ep.episode})"
              >${esc(ep.title || '—')}</span>
            </div>
            ${ep.air_date ? `<div class="ep-card-date">${esc(ep.air_date)}</div>` : ''}
          </div>
          <div class="ep-card-actions">
            <label class="ep-watched-toggle">
              <input type="checkbox" ${ep.watched ? 'checked' : ''} data-aired="${isAired(ep) ? 1 : 0}"
                     onchange="groupToggleWatched(${ep.show_id},${ep.season},${ep.episode},this.checked,${ep.season_total})">
              <span>Watched</span>
            </label>
            <button class="btn-sm" onclick="openEpisodeCast(${ep.show_id},${ep.season},${ep.episode},${epTitleJson})">Cast</button>
          </div>
        </div>`;
    }).join('');
    syncEvAllBox();
}

// Group lists don't prefetch IMDB IDs: open the popup at once with IMDB
// disabled, then switch it on when the (cached) lookup returns.
async function openGroupEpLink(event, tmdbUrl, showId, season, episode) {
    openEpLinkPopup(event, tmdbUrl, '');
    const r = await api('GET', `/api/shows/${showId}/episodes/${season}/${episode}/imdb`);
    const imdbEl = document.getElementById('ep-link-imdb');
    if (r.imdb_id && !document.getElementById('ep-link-popup').classList.contains('hidden')) {
        imdbEl.href = `https://www.imdb.com/title/${r.imdb_id}/`;
        imdbEl.classList.remove('ep-link-btn-disabled');
    }
}

async function groupToggleWatched(showId, season, episode, watched, seasonTotal) {
    await api('PUT', `/api/shows/${showId}/episodes/${season}/${episode}/watched`,
              { watched, season_total: seasonTotal });
    g_cal_cache = null;
    const card = document.getElementById(`gepcard-${showId}-${season}-${episode}`);
    if (card) card.classList.toggle('ep-card-watched', watched);
    syncEvAllBox();
    loadGroupPane();   // refresh watched counts
}

// ---------- Episode browser: season "All aired episodes" checkbox ----------
// One request via the season endpoint (same rules as the right-click menu:
// aired episodes only, position only moves forward). Season views only — a
// By year list spans several seasons and series.

function isAired(ep) {
    return !!ep.air_date && ep.air_date <= localDateStr(new Date());
}

// Built like an episode card (empty body, same actions area, an invisible
// stand-in for the Cast button) so its checkbox sits directly above the
// episodes' Watched checkboxes.
function evAllRowHtml(showId, season) {
    return `
      <div class="ep-card ev-all-card">
        <div class="ep-card-body"></div>
        <div class="ep-card-actions">
          <label class="ep-watched-toggle" title="Mark every aired episode in this season">
            <input type="checkbox" id="ev-all"
                   onchange="evSeasonAll(${showId},${season},this.checked)">
            <span>Watched All</span>
          </label>
          <button class="btn-sm ev-all-spacer" tabindex="-1" aria-hidden="true">Cast</button>
        </div>
      </div>`;
}

// Checked / indeterminate / empty from the aired episodes' own checkboxes
function syncEvAllBox() {
    const cb = document.getElementById('ev-all');
    if (!cb) return;
    const boxes = [...document.querySelectorAll('#ev-episodes-pane .ep-card input[type=checkbox][data-aired="1"]')];
    const n = boxes.filter(b => b.checked).length;
    cb.disabled      = boxes.length === 0;
    cb.checked       = boxes.length > 0 && n === boxes.length;
    cb.indeterminate = n > 0 && n < boxes.length;
}

async function evSeasonAll(showId, season, watched) {
    g_ctx_season = { showId, season };
    await markSeasonWatched(watched);   // refreshes both panes on success
    syncEvAllBox();                     // and puts the box back if it failed
}

// ---------- Season right-click menu (episode browser) -------------------------
// Whole-season watched/unwatched for a show's season: in the "Last watched"
// quick picker, the single-show browser, and a group's By season view.

let g_ctx_season = null;   // {showId, season} the open menu acts on

function seasonContextMenu(event, showId, season, watched, total) {
    event.preventDefault();
    event.stopPropagation();
    g_ctx_season = { showId, season };
    const items = [];
    if (total === 0 || watched < total)
        items.push(`<button class="menu-item" onclick="markSeasonWatched(true)">Mark season ${season} watched</button>`);
    if (watched > 0)
        items.push(`<button class="menu-item" onclick="markSeasonWatched(false)">Mark season ${season} unwatched</button>`);
    const menu = document.getElementById('season-ctx-menu');
    menu.innerHTML = items.join('');
    menu.classList.remove('hidden');
    // Keep the menu on screen near the pointer
    const w = menu.offsetWidth, h = menu.offsetHeight;
    menu.style.left = Math.max(4, Math.min(event.clientX, window.innerWidth  - w - 4)) + 'px';
    menu.style.top  = Math.max(4, Math.min(event.clientY, window.innerHeight - h - 4)) + 'px';
}

function closeSeasonMenu() {
    document.getElementById('season-ctx-menu').classList.add('hidden');
}

// Long-press = right-click on touch screens. iOS Safari never fires
// 'contextmenu', so a ~500 ms hold on a season button dispatches one to the
// button's own oncontextmenu handler. Moving the finger (a scroll) cancels;
// the tap that ends a long-press is swallowed so it doesn't also select.
const LONG_PRESS_MS   = 500;
const LONG_PRESS_MOVE = 10;   // px of movement that counts as a scroll
let g_lp_timer = null, g_lp_start = null, g_lp_fired = false;

function cancelLongPress() {
    clearTimeout(g_lp_timer);
    g_lp_timer = null;
}

document.addEventListener('touchstart', e => {
    g_lp_fired = false;
    const btn = e.target.closest('.season-btn[oncontextmenu]');
    if (!btn || e.touches.length !== 1) return;
    const t = e.touches[0];
    g_lp_start = { x: t.clientX, y: t.clientY };
    cancelLongPress();
    g_lp_timer = setTimeout(() => {
        g_lp_timer = null;
        g_lp_fired = true;
        btn.dispatchEvent(new MouseEvent('contextmenu', {
            bubbles: true, cancelable: true, clientX: g_lp_start.x, clientY: g_lp_start.y,
        }));
    }, LONG_PRESS_MS);
}, { passive: true });

document.addEventListener('touchmove', e => {
    if (!g_lp_timer || !g_lp_start) return;
    const t = e.touches[0];
    if (Math.abs(t.clientX - g_lp_start.x) > LONG_PRESS_MOVE ||
        Math.abs(t.clientY - g_lp_start.y) > LONG_PRESS_MOVE) cancelLongPress();
}, { passive: true });

document.addEventListener('touchend',    cancelLongPress, { passive: true });
document.addEventListener('touchcancel', cancelLongPress, { passive: true });

// Capture phase: runs before the button's onclick and the menu's outside-click close
document.addEventListener('click', e => {
    if (!g_lp_fired) return;
    g_lp_fired = false;
    e.preventDefault();
    e.stopPropagation();
}, true);

async function markSeasonWatched(watched) {
    if (!g_ctx_season) return;
    const { showId, season } = g_ctx_season;
    closeSeasonMenu();
    const r = await api('PUT', `/api/shows/${showId}/seasons/${season}/watched`, { watched });
    if (r.error) { alert('Could not update the season: ' + r.error); return; }
    g_cal_cache = null;

    if (g_popup_show_id === showId) {
        // Quick picker (modal over the main list): redraw it and the card behind
        const seasons = await api('GET', `/api/shows/${showId}/seasons`);
        if (!seasons.error) renderSeasons(showId, seasons, g_popup_season);
        if (g_popup_season === season) await loadPopupEpisodes(showId, season);
        loadShows();
    } else if (g_gv_group) {
        await loadGroupPane();
        if (g_gv_sel && g_gv_sel.show_id === showId && g_gv_sel.season === season)
            selectGroupSeason(showId, season);
    } else {
        const seasons = await api('GET', `/api/shows/${showId}/seasons`);
        if (!seasons.error)
            renderSeasons(showId, seasons, g_ev_season, 'ev-seasons-pane', 'selectEpisodeViewSeason');
        if (g_ev_season === season) selectEpisodeViewSeason(showId, season);
    }
}

// ---------- Settings modal ------------------------------------------------
// TMDB credentials and port are saved server-side (DB settings table, which
// overrides fi_config.json). Display prefs are per-browser (localStorage).
// Secrets are never sent back to the browser — only "set", last 4 chars and
// where the value came from.

let g_settings = null;   // last GET /api/settings response

async function openSettingsModal() {
    document.getElementById('menu-dropdown').classList.add('hidden');
    document.getElementById('settings-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    const body = document.getElementById('settings-body');
    body.innerHTML = '<div class="empty">Loading…</div>';
    g_settings = await api('GET', '/api/settings');
    if (g_settings.error) {
        body.innerHTML = `<div class="empty">Could not load settings: ${esc(g_settings.error)}</div>`;
        return;
    }
    renderSettings();
}

function closeSettingsModal() {
    document.getElementById('settings-modal').classList.add('hidden');
    document.body.style.overflow = '';
}

function settingsModalClick(event) {
    if (event.target === document.getElementById('settings-modal')) closeSettingsModal();
}

// Password field showing whether a secret is saved, plus where it came from
function secretFieldHtml(id, label, info) {
    const placeholder = info.set
        ? (info.hint ? `Saved — ends in …${info.hint}` : 'Saved')
        : 'Not set';
    const revert = info.source === 'settings'
        ? ` · <a href="#" onclick="clearSecretOverride('${id}');return false;">use config file value</a>`
        : '';
    return `
      <div class="field span2">
        <label for="set-${id}">${label}</label>
        <input type="password" id="set-${id}" placeholder="${esc(placeholder)}" autocomplete="off">
        <div class="settings-note">Source: ${esc(info.source)}${revert}</div>
      </div>`;
}

function renderSettings() {
    const s = g_settings;
    const zoomOpts = [];
    for (let z = ZOOM_MIN; z <= ZOOM_MAX + 0.001; z += ZOOM_STEP) {
        const v = +z.toFixed(2);
        zoomOpts.push(`<option value="${v}"${Math.abs(v - g_zoom) < 0.001 ? ' selected' : ''}>${Math.round(v * 100)}%</option>`);
    }
    const startTab   = localStorage.getItem('fi_startup_tab') || 'last';
    const startQueue = localStorage.getItem('fi_startup_queue') || 'last';
    const tabOpt = (v, label) => `<option value="${v}"${startTab === v ? ' selected' : ''}>${label}</option>`;
    const queueOpts = g_queues.map(q =>
        `<option value="${q.id}"${String(q.id) === startQueue ? ' selected' : ''}>${esc(q.name)}</option>`).join('');

    const portLocked  = s.port.locked;
    const restartNote = s.port.value !== s.port.running
        ? `<div class="settings-note settings-warn">Restart FlickImp to move from port ${s.port.running} to ${s.port.value}.</div>`
        : '';

    document.getElementById('settings-body').innerHTML = `
      <h3 class="settings-heading">TMDB</h3>
      <div class="form-grid">
        ${secretFieldHtml('tmdb_bearer_token', 'API Read Access Token (preferred)', s.tmdb_bearer_token)}
        ${secretFieldHtml('tmdb_api_key', 'API Key (v3, fallback)', s.tmdb_api_key)}
        <div class="span2 settings-inline">
          <button class="btn-sm" onclick="testTmdbSettings()">Test connection</button>
          <span id="settings-test-result" class="settings-note"></span>
        </div>
      </div>
      <div class="settings-note">Leave a field blank to keep the saved value.
        Get a token at <a href="https://www.themoviedb.org/settings/api" target="_blank" rel="noopener">themoviedb.org</a>.</div>

      <h3 class="settings-heading">Server</h3>
      <div class="form-grid">
        <div class="field">
          <label for="set-port">Web port</label>
          <input type="number" id="set-port" min="1" max="65535" value="${s.port.value}"${portLocked ? ' disabled' : ''}>
        </div>
        <div class="field settings-port-note">
          <div class="settings-note">${portLocked
              ? 'Set by --port on the command line; change it there.'
              : `Source: ${esc(s.port.source)}. Takes effect when FlickImp restarts.`}</div>
        </div>
        <div class="span2">${restartNote}</div>
      </div>

      <h3 class="settings-heading">Display <span class="settings-sub">(this browser only)</span></h3>
      <div class="form-grid">
        <div class="field">
          <label for="set-zoom">Zoom</label>
          <select id="set-zoom">${zoomOpts.join('')}</select>
        </div>
        <div class="field">
          <label for="set-startup-tab">Open on</label>
          <select id="set-startup-tab">
            ${tabOpt('last', 'Last used tab')}${tabOpt('shows', 'Shows')}${tabOpt('movies', 'Movies')}${tabOpt('calendar', 'Calendar')}
          </select>
        </div>
        <div class="field span2">
          <label for="set-startup-queue">Start in queue</label>
          <select id="set-startup-queue">
            <option value="last"${startQueue === 'last' ? ' selected' : ''}>Last used queue</option>
            ${queueOpts}
          </select>
        </div>
      </div>

      <div id="settings-save-result" class="settings-note"></div>
      <div class="form-actions">
        <button class="btn-cancel" onclick="closeSettingsModal()">Cancel</button>
        <button class="btn-primary" onclick="saveSettings()">Save</button>
      </div>`;
}

// Values typed into the two secret fields (blank = keep saved value)
function typedSecrets() {
    const body = {};
    for (const key of ['tmdb_bearer_token', 'tmdb_api_key']) {
        const v = document.getElementById(`set-${key}`).value.trim();
        if (v) body[key] = v;
    }
    return body;
}

async function testTmdbSettings() {
    const out = document.getElementById('settings-test-result');
    out.className = 'settings-note';
    out.textContent = 'Testing…';
    const r = await api('POST', '/api/settings/test-tmdb', typedSecrets());
    out.textContent = r.message || r.error || 'Unknown result';
    out.className = 'settings-note ' + (r.ok ? 'settings-ok' : 'settings-warn');
}

async function clearSecretOverride(key) {
    if (!confirm('Remove the value saved here and use the one from fi_config.json?')) return;
    g_settings = await api('PUT', '/api/settings', { [key]: '' });
    renderSettings();
}

async function saveSettings() {
    const out  = document.getElementById('settings-save-result');
    const body = typedSecrets();
    const portEl = document.getElementById('set-port');
    const port   = parseInt(portEl.value);
    if (!portEl.disabled && port !== g_settings.port.value) {
        if (!(port >= 1 && port <= 65535)) {
            out.className = 'settings-note settings-warn';
            out.textContent = 'Port must be between 1 and 65535.';
            return;
        }
        body.port = port;
    }

    // Per-browser display prefs
    g_zoom = parseFloat(document.getElementById('set-zoom').value);
    localStorage.setItem('fi_zoom', g_zoom);
    applyZoom();
    localStorage.setItem('fi_startup_tab', document.getElementById('set-startup-tab').value);
    const startQueue = document.getElementById('set-startup-queue').value;
    if (startQueue === 'last') localStorage.removeItem('fi_startup_queue');
    else localStorage.setItem('fi_startup_queue', startQueue);

    if (Object.keys(body).length) {
        const r = await api('PUT', '/api/settings', body);
        if (r.error) {
            out.className = 'settings-note settings-warn';
            out.textContent = 'Save failed: ' + r.error;
            return;
        }
        g_settings = r;
        if (r.port.value !== r.port.running) {
            renderSettings();
            const again = document.getElementById('settings-save-result');
            again.className = 'settings-note settings-ok';
            again.textContent = 'Saved. Restart FlickImp for the new port to take effect.';
            return;
        }
    }
    closeSettingsModal();
}

// ---------- Backup / Restore modal ---------------------------------------
// The daemon exports/imports plain JSON; gzip is done here with the
// browser's CompressionStream, so the server needs no compression library.

let g_backup_variants = null;   // { lite: {blob, counts}, full: {blob, counts} }
let g_backup_include  = new Set();  // PIN-protected queue ids to include (must be unlocked)
let g_backup_seq      = 0;          // ignore stale backup builds when boxes change quickly
let g_restore_text    = null;   // decompressed JSON text of the chosen file
let g_restore_data    = null;   // parsed backup

const CAN_GZIP = typeof CompressionStream !== 'undefined' && typeof DecompressionStream !== 'undefined';

function fmtBytes(n) {
    if (n < 1024) return `${n} B`;
    if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
    return `${(n / 1024 / 1024).toFixed(1)} MB`;
}

async function gzipText(text) {
    const stream = new Blob([text]).stream().pipeThrough(new CompressionStream('gzip'));
    return new Response(stream).blob();
}

async function gunzipBytes(buf) {
    const stream = new Blob([buf]).stream().pipeThrough(new DecompressionStream('gzip'));
    return new Response(stream).text();
}

function backupClientPrefs() {
    const prefs = {};
    for (const k of ['fi_zoom', 'fi_startup_tab', 'fi_startup_queue']) {
        const v = localStorage.getItem(k);
        if (v !== null) prefs[k] = v;
    }
    return prefs;
}

function backupCounts(b) {
    const n = (sec, t) => (b[sec] && Array.isArray(b[sec][t])) ? b[sec][t].length : 0;
    return {
        queues:  n('tables', 'queues'),
        shows:   n('tables', 'shows'),
        movies:  n('tables', 'movies'),
        watches: n('tables', 'episode_watches'),
        cast:    n('caches', 'show_cast') + n('caches', 'movie_cast') + n('caches', 'episode_cast'),
        people:  n('caches', 'people'),
    };
}

function countsLine(c) {
    return `${c.shows} shows · ${c.movies} movies · ${c.queues} queues · ${c.watches} watched episodes`;
}

async function openBackupModal() {
    document.getElementById('menu-dropdown').classList.add('hidden');
    document.getElementById('backup-modal').classList.remove('hidden');
    document.body.style.overflow = 'hidden';
    g_restore_text = g_restore_data = null;
    g_backup_include = new Set(g_queues.filter(q => q.has_pin && g_queue_tokens[q.id]).map(q => q.id));
    renderBackupModal();
    renderBackupPinQueues();
    await prepareBackupVariants();
}

function closeBackupModal() {
    document.getElementById('backup-modal').classList.add('hidden');
    document.body.style.overflow = '';
}

function backupModalClick(event) {
    if (event.target === document.getElementById('backup-modal')) closeBackupModal();
}

function renderBackupModal() {
    document.getElementById('backup-body').innerHTML = `
      <h3 class="settings-heading">Backup</h3>
      <div id="backup-pin-queues"></div>
      <div id="backup-options"><div class="empty">Preparing backup…</div></div>

      <h3 class="settings-heading">Restore</h3>
      <div class="settings-note">Choose a FlickImp backup file (.json.gz or .json).</div>
      <input type="file" id="restore-file" class="restore-file"
             accept=".gz,.json,application/gzip,application/json"
             onchange="restoreFileChosen(this)">
      <div id="restore-details"></div>`;
}

// Fetch the full backup once, then build both variants and compress each so
// the sizes shown are the real download sizes.
async function prepareBackupVariants() {
    const seq  = ++g_backup_seq;
    const box  = document.getElementById('backup-options');
    box.innerHTML = '<div class="empty">Preparing backup…</div>';
    const exclude = g_queues.filter(q => q.has_pin && !g_backup_include.has(q.id)).map(q => q.id);
    const full = await api('GET', `/api/backup?caches=1${exclude.length ? '&exclude=' + exclude.join(',') : ''}`);
    if (seq !== g_backup_seq) return;   // a newer build superseded this one
    if (full.error) {
        box.innerHTML = `<div class="empty">Could not create backup: ${esc(full.error)}</div>`;
        return;
    }
    full.client = backupClientPrefs();
    const lite = Object.assign({}, full);
    delete lite.caches;

    const buildVariant = async obj => {
        const text = JSON.stringify(obj);
        const blob = CAN_GZIP ? await gzipText(text) : new Blob([text], { type: 'application/json' });
        return { blob, counts: backupCounts(obj) };
    };
    const variants = { lite: await buildVariant(lite), full: await buildVariant(full) };
    if (seq !== g_backup_seq) return;
    g_backup_variants = variants;
    const lc = g_backup_variants.lite.counts, fc = g_backup_variants.full.counts;
    const leftOut = (full.excluded_queues || []).map(q => q.name);

    box.innerHTML = `
      <div class="settings-note">${countsLine(lc)}</div>
      ${leftOut.length ? `<div class="settings-note">Leaves out: ${esc(leftOut.join(', '))}</div>` : ''}
      <label class="choice-card">
        <input type="radio" name="backup-variant" value="lite" checked>
        <span class="choice-text">
          <span class="choice-title">Data and settings <span class="choice-size">${fmtBytes(g_backup_variants.lite.blob.size)}</span></span>
          <span class="choice-desc">Queues, shows, movies, watch history, TMDB credentials and display preferences.
            Cast lists and IMDB links are re-fetched from TMDB as needed after a restore.</span>
        </span>
      </label>
      <label class="choice-card">
        <input type="radio" name="backup-variant" value="full">
        <span class="choice-text">
          <span class="choice-title">Everything, including TMDB cache <span class="choice-size">${fmtBytes(g_backup_variants.full.blob.size)}</span></span>
          <span class="choice-desc">Also keeps ${fc.cast} cast entries for ${fc.people} people plus cached episode IMDB links,
            so a restored copy needs no TMDB lookups to show them.</span>
        </span>
      </label>
      <div class="settings-note settings-warn">The backup contains your TMDB credentials. Keep the file private.</div>
      ${CAN_GZIP ? '' : '<div class="settings-note settings-warn">This browser can’t compress files, so the backup will be plain JSON.</div>'}
      <div class="form-actions">
        <button class="btn-primary" onclick="downloadBackup()">Download backup</button>
      </div>`;
}

// PIN-protected queues: included only once unlocked; untick to leave one out
function renderBackupPinQueues() {
    const el = document.getElementById('backup-pin-queues');
    const pinQs = g_queues.filter(q => q.has_pin);
    if (!pinQs.length) { el.innerHTML = ''; return; }
    el.innerHTML = `
      <div class="settings-note">PIN-protected queues are included only after you enter their PIN.</div>
      ${pinQs.map(q => `
      <label class="check-row">
        <input type="checkbox" ${g_backup_include.has(q.id) ? 'checked' : ''}
               onchange="backupQueueToggle(${q.id}, this)">
        Include ${g_queue_tokens[q.id] ? '🔓' : '🔒'} ${esc(q.name)}
      </label>`).join('')}`;
}

async function backupQueueToggle(queueId, cb) {
    if (cb.checked) {
        if (!(await ensureUnlocked(queueId))) { cb.checked = false; return; }
        g_backup_include.add(queueId);
    } else {
        g_backup_include.delete(queueId);
    }
    renderBackupPinQueues();
    prepareBackupVariants();
}

function downloadBackup() {
    const which = document.querySelector('input[name="backup-variant"]:checked').value;
    const blob  = g_backup_variants[which].blob;
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = `flickimp-backup-${localDateStr(new Date())}${which === 'full' ? '-full' : ''}.json${CAN_GZIP ? '.gz' : ''}`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 10000);
}

async function restoreFileChosen(input) {
    const box = document.getElementById('restore-details');
    g_restore_text = g_restore_data = null;
    const file = input.files && input.files[0];
    if (!file) { box.innerHTML = ''; return; }
    try {
        const buf = new Uint8Array(await file.arrayBuffer());
        const gz  = buf.length > 2 && buf[0] === 0x1f && buf[1] === 0x8b;
        if (gz && !CAN_GZIP) throw new Error('this browser can’t decompress .gz files');
        const text = gz ? await gunzipBytes(buf) : new TextDecoder().decode(buf);
        const data = JSON.parse(text);
        if (data.format !== 'flickimp-backup') throw new Error('not a FlickImp backup file');
        g_restore_text = text;
        g_restore_data = data;
    } catch (err) {
        box.innerHTML = `<div class="settings-note settings-warn">Can’t read that file: ${esc(err.message)}</div>`;
        return;
    }

    const d = g_restore_data;
    const c = backupCounts(d);
    const created = d.created ? new Date(d.created).toLocaleString() : 'unknown date';
    const settingsKeys = Object.keys(d.settings || {});
    box.innerHTML = `
      ${(d.excluded_queues || []).length ? `
      <div class="settings-note settings-warn">This backup leaves out
        ${esc(d.excluded_queues.map(q => q.name).join(', '))}. Merge keeps those queues here;
        Replace deletes them on this server.</div>` : ''}
      <div class="restore-summary">
        <div><strong>${esc(created)}</strong> · FlickImp ${esc(d.app_version || '?')}</div>
        <div>${countsLine(c)}</div>
        <div>${d.caches ? `Includes TMDB cache (${c.cast} cast entries)` : 'No TMDB cache'}
             · ${settingsKeys.length ? 'Includes settings: ' + esc(settingsKeys.join(', ')) : 'No settings'}</div>
      </div>
      <label class="choice-card">
        <input type="radio" name="restore-mode" value="merge" checked>
        <span class="choice-text">
          <span class="choice-title">Merge</span>
          <span class="choice-desc">Add queues, shows and movies that aren’t already here. Anything already in the same
            queue is left alone, including its watch progress. Settings are only filled in where none are set.</span>
        </span>
      </label>
      <label class="choice-card">
        <input type="radio" name="restore-mode" value="replace">
        <span class="choice-text">
          <span class="choice-title">Replace</span>
          <span class="choice-desc">Delete all current queues, shows, movies and watch history and load the backup exactly.
            Settings in the backup overwrite current ones.</span>
        </span>
      </label>
      <div id="restore-result" class="settings-note"></div>
      <div class="form-actions">
        <button class="btn-danger" onclick="runRestore()">Restore</button>
      </div>`;
}

async function runRestore() {
    if (!g_restore_text) return;
    const mode = document.querySelector('input[name="restore-mode"]:checked').value;
    const out  = document.getElementById('restore-result');
    if (mode === 'replace') {
        for (const q of g_queues.filter(x => x.has_pin)) {
            if (!(await ensureUnlocked(q.id))) {
                out.className = 'settings-note settings-warn';
                out.textContent = `Replace deletes every queue, so each PIN-protected queue must be unlocked first (${q.name} wasn't).`;
                return;
            }
        }
    }
    const msg = mode === 'replace'
        ? 'Replace ALL current shows, movies, queues and watch history with this backup?\n\n'
          + 'A copy of the current database is saved next to it as flickimp.db.pre-restore.'
        : 'Merge this backup into your current data?';
    if (!confirm(msg)) return;

    out.className = 'settings-note';
    out.textContent = 'Restoring…';
    const res = await fetch(`/api/restore?mode=${mode}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: g_restore_text,
    });
    const r = await res.json();
    if (!res.ok || r.error) {
        out.className = 'settings-note settings-warn';
        out.textContent = 'Restore failed — nothing was changed. ' + (r.error || '');
        return;
    }

    // Per-browser prefs travel in the backup; apply them on a full replace only
    if (mode === 'replace' && g_restore_data.client) {
        for (const [k, v] of Object.entries(g_restore_data.client)) localStorage.setItem(k, v);
        g_zoom = parseFloat(localStorage.getItem('fi_zoom')) || 1;
        applyZoom();
    }

    const k = r.counts || {};
    const parts = mode === 'replace'
        ? [`${k.shows || 0} shows`, `${k.movies || 0} movies`, `${k.queues || 0} queues`,
           `${k.episode_watches || 0} watched episodes`]
        : [`${k.shows || 0} shows added (${k.shows_skipped || 0} already here)`,
           `${k.movies || 0} movies added (${k.movies_skipped || 0} already here)`,
           `${k.queues || 0} new queues`];
    if (r.settings_applied && r.settings_applied.length)
        parts.push('settings: ' + r.settings_applied.join(', '));
    out.className = 'settings-note settings-ok';
    out.textContent = 'Restored: ' + parts.join(' · ') + '.';

    g_cal_cache = null;
    await loadQueues();
    loadShows();
    loadMovies();
    if (g_main_tab === 'calendar') loadCalendar();
}

// ===== Release Calendar Module =====
// Data comes from TMDB via /api/shows/:id/episodes (one season per show), so
// the event list is cached in g_cal_cache and only refetched when it has been
// invalidated (show/movie list reloads, watched toggles) or on Refresh.

// Local-time YYYY-MM-DD (toISOString() would give the UTC date)
function localDateStr(d) {
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

function navigateCalendar(deltaMonths) {
    // Pin to the 1st first, or Jan 31 + 1 month rolls over into March
    g_cal_date.setDate(1);
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

    const todayStr = localDateStr(new Date());
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
        const dateStr = localDateStr(new Date(year, month, day));
        const isToday = dateStr === todayStr;
        const events = (g_cal_cache || []).filter(e => e.date === dateStr);

        const dow = dayHeaders[(firstDayIndex + day - 1) % 7];

        html += `
        <div class="calendar-day${isToday ? ' today' : ''}${events.length ? ' has-events' : ''}">
            <div class="day-top"><span class="day-dow">${dow}</span><span class="day-number">${day}</span></div>
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
// SN: 00006
