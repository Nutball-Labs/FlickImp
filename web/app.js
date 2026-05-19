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

// ---------- Shows — render -----------------------------------------------

async function loadShows() {
    const shows = await api('GET', '/api/shows');
    renderShows(shows);
}

function showStatusBadge(status) {
    const map = {
        watching: ['badge-watching', 'FlickImp'],
        paused:   ['badge-paused',   'Paused'],
        finished: ['badge-finished', 'Finished'],
    };
    const [cls, label] = map[status] ?? ['badge-watching', status];
    return `<span class="badge ${cls}">${label}</span>`;
}

function showCard(s) {
    const notesHtml  = s.notes
        ? `<div class="card-notes">${esc(s.notes)}</div>` : '';
    const pauseLabel = s.status === 'paused' ? '▶ Resume' : '⏸ Pause';
    const thumbHtml  = s.thumbnail_url
        ? `<img class="card-thumb" src="${esc(s.thumbnail_url)}" alt="" loading="lazy">` : '';
    const titleJson  = JSON.stringify(s.title).replace(/"/g, '&quot;');

    return `
    <div class="show-wrap" id="wrap-${s.id}">

      <!-- ── Main card ──────────────────────────────────────────────── -->
      <div class="card" data-id="${s.id}">
        ${thumbHtml}
        <div class="card-body">
          <div class="card-top">
            <div class="card-title">${esc(s.title)}</div>
            ${showStatusBadge(s.status)}
          </div>
          ${s.service ? `<div class="card-meta"><span class="service-tag">${esc(s.service)}</span></div>` : ''}
          <div class="ep-track">
            <div class="ep-last-watched"
                 onclick="openWatchedPopup(${s.id}, ${titleJson}, ${s.season})">
              Last watched: S${pad3(s.season)} &minus; E${pad3(s.episode)}
            </div>
            <div class="ep-next-watch">Next watch: S${pad3(s.season)} &minus; E${pad3(s.episode + 1)}</div>
          </div>
          ${notesHtml}
          <div class="card-actions">
            <button class="btn-action"
                    onclick="cycleStatus(${s.id},'${s.status}')">${pauseLabel}</button>
            <button class="btn-sm" onclick="toggleEdit(${s.id})">Edit</button>
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

function renderShows(shows) {
    const list = document.getElementById('shows-list');
    if (!shows.length) {
        list.innerHTML = '<div class="empty">No shows yet — add one above.</div>';
        return;
    }
    list.innerHTML = shows.map(showCard).join('');
}

// ---------- Shows — card actions -----------------------------------------

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
    document.getElementById('add-show-form').classList.remove('hidden');
    document.getElementById('new-show-title').focus();
}
function hideAddShowForm() {
    document.getElementById('add-show-form').classList.add('hidden');
    clearShowForm();
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

    const raw = document.getElementById('new-show-ep').value.trim() || 's001-e001';
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

function renderSeasons(showId, seasons, activeSeason) {
    const pane = document.getElementById('ep-seasons-pane');
    if (!seasons.length) {
        pane.innerHTML = '<div class="empty">No seasons found.</div>';
        return;
    }
    pane.innerHTML = seasons.map(s => {
        const allWatched = s.total_episodes > 0 && s.watched_count >= s.total_episodes;
        return `
      <div class="season-slot">
        <button class="season-btn ${seasonClass(s.watched_count, s.total_episodes)}${s.season === activeSeason ? ' season-active' : ''}"
                id="sbtn-${showId}-${s.season}"
                onclick="selectSeason(${showId}, ${s.season})">
          S${String(s.season).padStart(2,'0')}
          <span class="season-ep-count">${s.watched_count}/${s.total_episodes}</span>
        </button>
        <label class="season-all-label" title="Mark all episodes watched/unwatched">
          <input type="checkbox" class="season-all-check"
                 id="sall-${showId}-${s.season}"
                 ${allWatched ? 'checked' : ''}
                 onchange="seasonCheckAll(${showId}, ${s.season}, this)">
        </label>
      </div>`;
    }).join('');
    // indeterminate can only be set via DOM, not HTML attribute
    seasons.forEach(s => {
        const partial = s.watched_count > 0 && s.watched_count < s.total_episodes;
        const cb = document.getElementById(`sall-${showId}-${s.season}`);
        if (cb) cb.indeterminate = partial;
    });
}

async function seasonCheckAll(showId, season, checkbox) {
    const watched = checkbox.checked;
    checkbox.disabled = true;
    try {
        const episodes = await api('GET', `/api/shows/${showId}/episodes?season=${season}`);
        if (!Array.isArray(episodes) || episodes.error) return;
        await Promise.all(
            episodes.map(ep =>
                api('PUT', `/api/shows/${showId}/episodes/${season}/${ep.episode}/watched`, {watched})
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
    pane.innerHTML = episodes.map(e => `
      <div class="popup-ep-row${e.watched ? ' ep-watched' : ''}"
           id="pep-${showId}-${e.season}-${e.episode}">
        <input type="checkbox" ${e.watched ? 'checked' : ''}
               onchange="popupMarkWatched(${showId},${e.season},${e.episode},this.checked)">
        <span class="ep-code">E${String(e.episode).padStart(3,'0')}</span>
        <span class="ep-title">${esc(e.title || '—')}</span>
        <span class="ep-date">${e.air_date || ''}</span>
      </div>`).join('');
}

async function popupMarkWatched(showId, season, episode, watched) {
    const data = await api('PUT',
        `/api/shows/${showId}/episodes/${season}/${episode}/watched`, {watched});

    const row = document.getElementById(`pep-${showId}-${season}-${episode}`);
    if (row) row.classList.toggle('ep-watched', watched);

    if (data.show) {
        const show = data.show;
        const wrap = document.getElementById(`wrap-${showId}`);
        if (wrap) {
            const lw = wrap.querySelector('.ep-last-watched');
            const nw = wrap.querySelector('.ep-next-watch');
            if (lw) lw.textContent =
                `Last watched: S${pad3(show.season)} − E${pad3(show.episode)}`;
            if (nw) nw.textContent =
                `Next watch: S${pad3(show.season)} − E${pad3(show.episode + 1)}`;
        }
    }

    const seasons = await api('GET', `/api/shows/${showId}/seasons`);
    if (!seasons.error) renderSeasons(showId, seasons, g_popup_season);
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
    const notesHtml  = m.notes
        ? `<div class="card-notes">${esc(m.notes)}</div>` : '';
    const dateHtml   = m.release_date
        ? `<div class="card-meta"><span class="release-date">Released: ${esc(m.release_date)}</span></div>` : '';
    const thumbHtml  = m.thumbnail_url
        ? `<img class="card-thumb" src="${esc(m.thumbnail_url)}" alt="" loading="lazy">` : '';
    const watchedLabel = m.status === 'watched' ? '↩ Unwatch' : '✓ Watched';

    return `
    <div class="card" data-id="${m.id}">
      ${thumbHtml}
      <div class="card-body">
      <div class="card-top">
        <div class="card-title">${esc(m.title)}</div>
        ${movieStatusBadge(m.status)}
      </div>
      ${dateHtml}
      ${notesHtml}
      <div class="card-actions">
        <button class="btn-watched" onclick="toggleWatched(${m.id},'${m.status}')">
          ${watchedLabel}
        </button>
        <button class="btn-danger" onclick="deleteMovie(${m.id})">Remove</button>
      </div>
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
    });
    hideAddMovieForm();
    loadMovies();
}

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

document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && g_popup_show_id) closeWatchedPopup();
    if (e.key === 'Escape' && !document.getElementById('check-modal').classList.contains('hidden'))
        closeCheckModal();
});

loadShows();
loadMovies();

// SN: 00003
