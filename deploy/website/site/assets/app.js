/* SPDX-License-Identifier: Apache-2.0 */
(() => {
  'use strict';
  const root = document.documentElement;
  root.classList.add('js');
  const storage = {
    get(key) { try { return localStorage.getItem(key); } catch { return null; } },
    set(key, value) { try { localStorage.setItem(key, value); } catch { /* Theme remains usable without storage. */ } }
  };
  const themeButton = document.querySelector('[data-theme-toggle]');
  let theme = storage.get('lilinng-theme');
  if (theme !== 'dark' && theme !== 'light') theme = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  function applyTheme(value) {
    root.dataset.theme = value;
    themeButton?.setAttribute('aria-label', value === 'dark' ? '切换浅色主题' : '切换深色主题');
    themeButton?.setAttribute('title', value === 'dark' ? '切换浅色主题' : '切换深色主题');
  }
  applyTheme(theme);
  themeButton?.addEventListener('click', () => {
    theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
    storage.set('lilinng-theme', theme);
    applyTheme(theme);
  });

  const toast = document.querySelector('#toast');
  let toastTimer;
  document.querySelectorAll('[data-copy]').forEach(button => button.addEventListener('click', async () => {
    const text = document.querySelector(button.dataset.copy).textContent.trim();
    let copied = false;
    try {
      if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(text); copied = true; }
    } catch { /* HTTP deployments use the local selection fallback. */ }
    if (!copied) {
      const area = document.createElement('textarea');
      area.value = text;
      area.className = 'clipboard-helper';
      document.body.append(area);
      area.select();
      try { copied = document.execCommand('copy'); } catch { copied = false; }
      area.remove();
      button.focus();
    }
    toast.textContent = copied ? '校验值已复制' : '复制未成功，请选中校验值手动复制';
    toast.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { toast.hidden = true; }, 3000);
  }));

  function renderInline(target, text) {
    // Build DOM nodes rather than interpreting release text as HTML.
    for (const part of text.split(/(\*\*[^*]+\*\*|`[^`]+`)/g)) {
      if (part.startsWith('**') && part.endsWith('**')) {
        const node = document.createElement('strong');
        node.textContent = part.slice(2, -2);
        target.append(node);
      } else if (part.startsWith('`') && part.endsWith('`')) {
        const node = document.createElement('code');
        node.textContent = part.slice(1, -1);
        target.append(node);
      } else {
        target.append(document.createTextNode(part));
      }
    }
  }

  function renderReleaseNotes(target, text) {
    target.replaceChildren();
    let paragraph = [];
    let list = null;
    let code = null;
    const flush = () => {
      if (paragraph.length) {
        const node = document.createElement('p');
        renderInline(node, paragraph.join(' '));
        target.append(node);
        paragraph = [];
      }
    };
    for (const line of text.split(/\r?\n/)) {
      if (line.startsWith('```')) {
        flush(); list = null;
        if (code) { code = null; }
        else { code = document.createElement('pre'); target.append(code); }
        continue;
      }
      if (code) { code.textContent += line + '\n'; continue; }
      const heading = line.match(/^#{1,6}\s+(.+)$/);
      const bullet = line.match(/^\s*(?:[-*]|\d+\.)\s+(.+)$/);
      if (heading) {
        flush(); list = null;
        const node = document.createElement('h3'); renderInline(node, heading[1]); target.append(node);
      } else if (bullet) {
        flush();
        if (!list) { list = document.createElement('ul'); target.append(list); }
        const node = document.createElement('li'); renderInline(node, bullet[1]); list.append(node);
      } else if (!line.trim()) { flush(); list = null; }
      else { list = null; paragraph.push(line.trim()); }
    }
    flush();
  }

  let releaseLoading = false;
  let releaseLoaded = false;
  async function loadLatestRelease() {
    if (!document.querySelector('[data-release-version]')) return;
    if (releaseLoading) return;
    releaseLoading = true;
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 8000);
    try {
      const response = await fetch('/updates/latest.json', { cache: 'no-store', signal: controller.signal });
      if (!response.ok) throw new Error('Release mirror unavailable');
      const release = await response.json();
      if (!/^\d+\.\d+\.\d+$/.test(release.version) || !/^[0-9a-f]{64}$/i.test(release.sha256)) throw new Error('Invalid release');
      const filename = `SerialWorkbench-${release.version}-windows-x64-setup.exe`;
      const download = new URL(release.url);
      if (release.filename !== filename || download.protocol !== 'https:' || !['162.14.72.194', 'www.lilinng.cn', 'lilinng.cn', location.hostname].includes(download.hostname)
          || download.pathname !== '/updates/' + filename || download.search || download.hash || download.username || download.password
          || !Number.isSafeInteger(release.size) || release.size <= 0 || release.size > 300 * 1024 * 1024) throw new Error('Invalid installer');
      document.querySelectorAll('[data-release-version]').forEach(node => { node.textContent = 'v' + release.version; });
      document.querySelectorAll('[data-release-download]').forEach(node => { node.href = '/updates/' + filename; node.download = filename; });
      document.querySelectorAll('[data-release-size]').forEach(node => { node.textContent = (release.size / 1048576).toFixed(1) + ' MiB'; });
      const date = new Date(release.published_at);
      if (!Number.isNaN(date.getTime())) document.querySelectorAll('[data-release-date]').forEach(node => { node.textContent = '更新于 ' + date.toLocaleDateString('zh-CN'); });
      document.querySelectorAll('[data-release-checksum]').forEach(node => { node.textContent = release.sha256; });
      document.querySelectorAll('[data-release-checksum-file]').forEach(node => { node.href = '/updates/' + filename + '.sha256'; });
      const githubBase = 'https://github.com/lilinng-163/serial-workbench/releases/';
      document.querySelectorAll('[data-release-github]').forEach(node => { node.href = githubBase + 'download/v' + release.version + '/' + filename; });
      document.querySelectorAll('[data-release-page]').forEach(node => { node.href = githubBase + 'tag/v' + release.version; });
      document.querySelectorAll('[data-release-notes]').forEach(node => { renderReleaseNotes(node, release.notes || '此版本未提供更新说明。'); });
      releaseLoaded = true;
    } catch {
      if (!releaseLoaded) document.querySelectorAll('[data-release-notes]').forEach(node => { node.textContent = '暂时无法读取最新说明，可以通过上方的 Release 链接查看。'; });
    } finally { clearTimeout(timeout); releaseLoading = false; }
  }
  loadLatestRelease();
  // Keep an already-open page current after the server publishes a new mirror.
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden) loadLatestRelease();
  });
  setInterval(() => { if (!document.hidden) loadLatestRelease(); }, 60000);

  const stage = document.querySelector('[data-lab-stage]');
  const motionAllowed = matchMedia('(prefers-reduced-motion: no-preference) and (pointer: fine)');
  if (stage) {
    let frame = 0;
    let tiltX = 0;
    let tiltY = 0;
    let lightX = 25;
    let lightY = 10;
    stage.addEventListener('pointermove', event => {
      if (!motionAllowed.matches) return;
      const rect = stage.getBoundingClientRect();
      tiltY = ((event.clientX - rect.left) / rect.width - .5) * 5;
      tiltX = -((event.clientY - rect.top) / rect.height - .5) * 4;
      lightX = (event.clientX - rect.left) / rect.width * 100;
      lightY = (event.clientY - rect.top) / rect.height * 100;
      if (!frame) frame = requestAnimationFrame(() => {
        stage.style.setProperty('--tilt-x', tiltX + 'deg');
        stage.style.setProperty('--tilt-y', tiltY + 'deg');
        stage.style.setProperty('--light-x', lightX + '%');
        stage.style.setProperty('--light-y', lightY + '%');
        frame = 0;
      });
    });
    const resetStage = () => {
      cancelAnimationFrame(frame);
      frame = 0;
      stage.style.setProperty('--tilt-x', '0deg');
      stage.style.setProperty('--tilt-y', '0deg');
      stage.style.removeProperty('--light-x');
      stage.style.removeProperty('--light-y');
    };
    stage.addEventListener('pointerleave', resetStage);
    motionAllowed.addEventListener('change', resetStage);
  }
})();
