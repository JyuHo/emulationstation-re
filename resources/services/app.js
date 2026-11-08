// EmulationStation-RE web pages: shared sidebar, top bar, API helper and notifications.
// Each page calls ES.shell("<page id>", "<title>") and then fills <main id="content">.
// REST API: see the list at the top of es-app/src/services/HttpServerThread.cpp

var ES = (function() {

	// Line icons, 24x24, drawn with currentColor
	var ICONS = {
		dashboard: '<rect x="3" y="3" width="7" height="9" rx="1"/><rect x="14" y="3" width="7" height="5" rx="1"/><rect x="14" y="12" width="7" height="9" rx="1"/><rect x="3" y="16" width="7" height="5" rx="1"/>',
		games: '<rect x="2" y="7" width="20" height="11" rx="5"/><path d="M7 11v3M5.5 12.5h3"/><circle cx="15.5" cy="11.5" r="1"/><circle cx="18" cy="13.5" r="1"/>',
		files: '<path d="M3 6a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
		file: '<path d="M14 3H6a1 1 0 0 0-1 1v16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8z"/><path d="M14 3v5h5"/>',
		reload: '<path d="M20 11a8 8 0 1 0-2.3 5.7"/><path d="M20 4v7h-7"/>',
		stop: '<rect x="6" y="6" width="12" height="12" rx="1"/>',
		power: '<path d="M12 3v9"/><path d="M6.3 7a8 8 0 1 0 11.4 0"/>',
		restart: '<path d="M4 12a8 8 0 1 0 2.3-5.7"/><path d="M4 4v5h5"/>',
		play: '<path d="M7 4l13 8-13 8z"/>',
		systems: '<rect x="4" y="3" width="16" height="18" rx="2"/><path d="M8 7h8M8 11h8"/><circle cx="12" cy="16" r="1.5"/>',
		star: '<path d="M12 3l2.8 5.7 6.2.9-4.5 4.4 1 6.2L12 17.3 6.5 20.2l1-6.2L3 9.6l6.2-.9z"/>',
		trophy: '<path d="M8 4h8v5a4 4 0 0 1-8 0z"/><path d="M8 6H5a3 3 0 0 0 3 4M16 6h3a3 3 0 0 1-3 4M12 13v4M8 20h8M9 17h6"/>',
		upload: '<path d="M12 16V4M7 9l5-5 5 5"/><path d="M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3"/>',
		folderPlus: '<path d="M3 6a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><path d="M12 10v6M9 13h6"/>',
		search: '<circle cx="11" cy="11" r="7"/><path d="M20 20l-4-4"/>',
		menu: '<path d="M4 6h16M4 12h16M4 18h16"/>',
		activity: '<path d="M3 12h4l3-8 4 16 3-8h4"/>'
	};

	function icon(name) {
		return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + (ICONS[name] || "") + '</svg>';
	}

	function escapeHtml(s) {
		return String(s == null ? "" : s).replace(/[&<>"']/g, function(c) {
			return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
		});
	}

	// Every call goes through here: the browser asks for the password itself when the API answers 401
	function api(method, url, body) {
		return fetch(url, { method: method || "GET", body: body, credentials: "same-origin" }).then(function(res) {
			if (res.ok)
				return res;

			return res.json().catch(function() { return {}; }).then(function(json) {
				throw new Error(json.error || (res.status + " " + res.statusText));
			});
		});
	}

	function getJson(url) {
		return api("GET", url).then(function(res) { return res.json(); });
	}

	var toastTimer = null;
	function toast(text, isError) {
		var el = document.getElementById("toast");
		el.textContent = text;
		el.className = "toast show" + (isError ? " error" : "");
		clearTimeout(toastTimer);
		toastTimer = setTimeout(function() { el.className = "toast" + (isError ? " error" : ""); }, 3500);
	}

	// Sidebar actions that only call the API
	var ACTIONS = {
		reload: { url: "/reloadgames", done: "Reloading the game lists..." },
		emukill: { url: "/emukill", done: "Emulator stopped" },
		quit: { url: "/quit", confirm: "Quit EmulationStation?", done: "EmulationStation is quitting" },
		restart: { url: "/restart", confirm: "Restart the computer?", done: "Restarting..." }
	};

	function run(name) {
		var action = ACTIONS[name];
		if (action.confirm && !confirm(action.confirm))
			return Promise.resolve();

		return api("GET", action.url).then(function() {
			toast(action.done);
			if (name == "emukill")
				setTimeout(refreshStatus, 1000);
		}).catch(function(e) { toast(e.message, true); });
	}

	var NAV = [
		{ id: "dashboard", label: "Dashboard", href: "index.html", icon: "dashboard" },
		{ id: "games", label: "Games", href: "games.html", icon: "games" },
		{ id: "files", label: "ROMs, BIOS & configs", href: "files.html", icon: "files" }
	];

	var CONTROLS = [
		{ action: "reload", label: "Reload games", icon: "reload" },
		{ action: "emukill", label: "Stop emulator", icon: "stop" },
		{ action: "quit", label: "Quit EmulationStation", icon: "power", danger: true },
		{ action: "restart", label: "Restart computer", icon: "restart", danger: true }
	];

	function shell(page, title) {
		var nav = NAV.map(function(item) {
			return '<a class="nav-item' + (item.id == page ? ' active' : '') + '" href="' + item.href + '">' + icon(item.icon) + '<span>' + escapeHtml(item.label) + '</span></a>';
		}).join("");

		var controls = CONTROLS.map(function(item) {
			return '<button class="nav-item' + (item.danger ? ' danger' : '') + '" data-action="' + item.action + '">' + icon(item.icon) + '<span>' + escapeHtml(item.label) + '</span></button>';
		}).join("");

		var content = document.getElementById("content");

		var app = document.createElement("div");
		app.className = "app";
		app.innerHTML =
			'<aside class="sidebar">' +
				'<a class="brand" href="index.html"><img src="/resources/splash.svg" alt="">' +
					'<div><div class="brand-name">ES-RE</div><div class="brand-sub">EmulationStation-RE</div></div></a>' +
				'<nav class="nav"><div class="nav-title">Menu</div>' + nav + '<div class="nav-title">Controls</div>' + controls + '</nav>' +
				'<div class="sidebar-foot" id="version"></div>' +
			'</aside>' +
			'<div class="main">' +
				'<header class="topbar">' +
					'<button class="menu-toggle" aria-label="Menu">' + icon("menu") + '</button>' +
					'<h1>' + escapeHtml(title) + '</h1><div class="spacer"></div>' +
					'<span class="pill" id="status"><span class="dot"></span><span>...</span></span>' +
				'</header>' +
			'</div>' +
			'<div class="toast" id="toast"></div>';

		document.body.insertBefore(app, document.body.firstChild);
		app.querySelector(".main").appendChild(content);
		content.classList.add("content");

		app.querySelectorAll("[data-action]").forEach(function(el) {
			el.onclick = function() { run(el.dataset.action); };
		});
		app.querySelector(".menu-toggle").onclick = function(e) {
			e.stopPropagation();
			app.classList.toggle("nav-open");
		};
		content.addEventListener("click", function() { app.classList.remove("nav-open"); });

		document.title = title + " - EmulationStation-RE";

		getJson("/caps").then(function(caps) {
			if (caps.Version)
				document.getElementById("version").textContent = "Version " + caps.Version;
		}).catch(function() {});

		refreshStatus();
		setInterval(refreshStatus, 10000);
	}

	var statusListeners = [];

	// GET /runningGame answers 201 with a message when nothing runs, 200 with the game otherwise
	function refreshStatus() {
		return fetch("/runningGame", { credentials: "same-origin" }).then(function(res) {
			return res.json().then(function(json) { return res.status == 200 ? json : null; });
		}).catch(function() { return undefined; }).then(function(game) {
			var el = document.getElementById("status");
			if (game === undefined)
				el.innerHTML = '<span class="dot"></span><span>Offline</span>';
			else if (game)
				el.innerHTML = '<span class="dot busy"></span><span>Playing: ' + escapeHtml(game.name) + '</span>';
			else
				el.innerHTML = '<span class="dot ok"></span><span>Ready</span>';

			statusListeners.forEach(function(fn) { fn(game); });
		});
	}

	function onStatus(fn) {
		statusListeners.push(fn);
	}

	function launch(path) {
		return api("POST", "/launch", path).then(function() {
			toast("Starting the game...");
			setTimeout(refreshStatus, 2000);
		}).catch(function(e) { toast(e.message, true); });
	}

	function byName(a, b) {
		return String(a.name).localeCompare(String(b.name), undefined, { numeric: true, sensitivity: "base" });
	}

	return {
		icon: icon,
		escapeHtml: escapeHtml,
		api: api,
		getJson: getJson,
		toast: toast,
		run: run,
		shell: shell,
		onStatus: onStatus,
		refreshStatus: refreshStatus,
		launch: launch,
		byName: byName
	};
})();
