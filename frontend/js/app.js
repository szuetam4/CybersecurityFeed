let currentSort = 'desc';
let currentSearch = '';
let currentCategory = '';

const PAGE_SIZE = 20;
let currentOffset = 0;
let hasMoreArticles = true;
let isLoadingArticles = false;
let scrollObserver = null;

const PLACEHOLDER_COLORS = ['img-tech', 'img-news', 'img-violet', 'img-amber', 'img-teal', 'img-crimson'];

// Prosty hash tekstu (32-bit) - deterministyczny, żeby dany artykuł zawsze
// dostawał ten sam kolor placeholdera niezależnie od odświeżenia strony.
function hashString(str) {
	let hash = 0;
	for (let i = 0; i < str.length; i++) {
		hash = (hash << 5) - hash + str.charCodeAt(i);
		hash |= 0; // wymuszenie 32-bit int
	}
	return Math.abs(hash);
}

function getPlaceholderColorClass(article) {
	const key = article.link || String(article.id);
	const hash = hashString(key);
	return PLACEHOLDER_COLORS[hash % PLACEHOLDER_COLORS.length];
}

// Walidacja protokołu - blokujemy javascript:, data: itd.
function isSafeUrl(url) {
	try {
		const u = new URL(url, window.location.origin);
		return ['http:', 'https:'].includes(u.protocol);
	} catch { return false; }
}

// Wydzielone z loadArticles, żeby ta sama logika renderowania karty służyła
// zarówno pierwszemu załadowaniu, jak i doładowywaniu kolejnych stron (infinite scroll).
function createArticleCard(article) {
	const dateObj = new Date(article.published_at);
	const formattedDate = dateObj.toLocaleDateString('pl-PL', {
		day: 'numeric',
		month: 'long',
		hour: '2-digit',
		minute: '2-digit'
	});

	const card = document.createElement('article');
	card.className = 'article-card';

	const a = document.createElement('a');
	a.href = isSafeUrl(article.link) ? article.link : '#';

	const imgWrapper = document.createElement('div');
	imgWrapper.className = `card-image-placeholder ${getPlaceholderColorClass(article)}`;

	const hasValidImage = isSafeUrl(article.img_url) && article.img_url.trim() !== '';

	if (hasValidImage) {
		const img = document.createElement('img');
		img.className = 'card-image';
		img.src = article.img_url;
		img.alt = article.title; // opisowy alt zamiast generycznego "article image"
		imgWrapper.appendChild(img);
	}

	const badge = document.createElement('span');
	badge.className = 'category-badge';
	badge.textContent = article.tags; // textContent = brak interpretacji HTML

	const content = document.createElement('div');
	content.className = 'card-content';

	const source = document.createElement('div');
	source.className = 'source-name';
	source.textContent = article.source_name;

	const title = document.createElement('h2');
	title.className = 'card-title';
	title.textContent = article.title; // bezpieczne

	const footer = document.createElement('div');
	footer.className = 'card-footer';
	const time = document.createElement('time');
	time.textContent = formattedDate;
	const more = document.createElement('span');
	more.className = 'read-more';
	more.textContent = 'Czytaj dalej →';
	footer.append(time, more);

	content.append(source, title, footer);
	imgWrapper.append(badge);
	a.append(imgWrapper, content);
	card.append(a);

	return card;
}

async function fetchArticles(offset, { category, sort, search }) {
	const API_URL = new URL('/api/articles.php', window.location.origin);
	if (category) API_URL.searchParams.append('category', category);
	if (search) API_URL.searchParams.append('q', search);
	API_URL.searchParams.append('sort', sort);
	API_URL.searchParams.append('limit', PAGE_SIZE);
	API_URL.searchParams.append('offset', offset);

	const response = await fetch(API_URL);

	if (!response.ok) {
		throw new Error('Błąd połączenia z API (Status: ' + response.status + ')');
	}

	return response.json();
}

function setLoadingIndicator(feedContainer, show) {
	let indicator = document.getElementById('feed-loading-indicator');

	if (show) {
		if (!indicator) {
			indicator = document.createElement('div');
			indicator.id = 'feed-loading-indicator';
			indicator.className = 'feed-status-message';
			indicator.textContent = 'Ładowanie...';
			feedContainer.appendChild(indicator);
		}
	} else {
		indicator?.remove();
	}
}

function showLoadMoreError(feedContainer) {
	const errorMsg = document.createElement('div');
	errorMsg.className = 'feed-status-message';
	errorMsg.textContent = 'Nie udało się wczytać kolejnych artykułów.';
	feedContainer.appendChild(errorMsg);
}
// reset = true: nowe filtry (kategoria/sortowanie/wyszukiwanie) - czyścimy feed i zaczynamy od offsetu 0.
// reset = false: infinite scroll - dociągamy kolejną stronę i dopisujemy do istniejącego feedu.
async function loadArticles({ reset = true } = {}) {
	const feedContainer = document.getElementById('news-feed');

	if (isLoadingArticles) return;
	if (!reset && !hasMoreArticles) return;

	isLoadingArticles = true;

	if (reset) {
		currentOffset = 0;
		hasMoreArticles = true;
		feedContainer.innerHTML = '';
	}

	setLoadingIndicator(feedContainer, true);

	try {
		const result = await fetchArticles(currentOffset, {
			category: currentCategory,
			sort: currentSort,
			search: currentSearch
		});

		const articles = result.data;

		setLoadingIndicator(feedContainer, false);

		if (reset && articles.length === 0) {
			feedContainer.innerHTML = 'Brak artykułów w bazie.';
			hasMoreArticles = false;
			return;
		}

		articles.forEach(article => {
			feedContainer.appendChild(createArticleCard(article));
		});

		currentOffset += articles.length;
		hasMoreArticles = result.has_more === true;

	} catch (error) {
		console.error('Krytyczny bład pobierania:', error);
		setLoadingIndicator(feedContainer, false);

		if (reset) {
			feedContainer.innerHTML = 'Nie udało się połaczyć z API, sprawdź czy serwer PHP działa w tle.';
		} else {
			showLoadMoreError(feedContainer);
		}
		// Zatrzymujemy dalsze automatyczne próby do czasu zmiany filtrów (reset),
		// żeby błąd sieci nie wywołał pętli powtarzanych żądań przy każdym scrollu.
		hasMoreArticles = false;
	} finally {
		isLoadingArticles = false;
	}
}

function loadMoreArticles() {
	loadArticles({ reset: false });
}

// Sentinel wstawiany programowo za feed-container (nie w index.html), żeby
// IntersectionObserver mógł wykryć zbliżanie się użytkownika do końca listy
// bez potrzeby liczenia pozycji scrolla ręcznie.
function ensureScrollSentinel() {
	let sentinel = document.getElementById('feed-scroll-sentinel');
	if (!sentinel) {
		sentinel = document.createElement('div');
		sentinel.id = 'feed-scroll-sentinel';
		sentinel.className = 'feed-scroll-sentinel';
		sentinel.setAttribute('aria-hidden', 'true');
		document.getElementById('news-feed').after(sentinel);
	}
	return sentinel;
}

function setupInfiniteScroll() {
	const sentinel = ensureScrollSentinel();

	if (scrollObserver) scrollObserver.disconnect();

	scrollObserver = new IntersectionObserver((entries) => {
		if (entries[0].isIntersecting) {
			loadMoreArticles();
		}
	}, { rootMargin: '200px' });

	scrollObserver.observe(sentinel);
}

async function loadCategories() {
	const categoriesList = document.getElementById('category-filter');
	const API_URL = new URL('/api/categories.php', window.location.origin);

	try {
		const response = await fetch(API_URL);

		if (!response.ok) {
			throw new Error('Bład połączenia z API (Status: ' + response.status + ')');
		}

		const result = await response.json();
		const categories = result.data;

		categories.forEach(category => {
			const tag = document.createElement('option');
			tag.textContent = `${category.name}`;
			tag.value = `${category.name}`;

			categoriesList.appendChild(tag);
		});
	} catch (error) {
		console.error('Krytyczny błąd pobierania:', error);
		categoriesList.innerHTML = 'Nie udało się połączyć z API, sprawdź czy serwer PHP działa w tle.';
	}
}

function sortToggle() {
	let sortButton = document.getElementById('sort-toggle');

	if (currentSort === 'desc') {
		currentSort = 'asc';
		sortButton.textContent = 'Najnowsze';
	} else {
		currentSort = 'desc';
		sortButton.textContent = 'Najstarsze';
	}

	loadArticles({ reset: true });
}

function handleCategoryChange(event) {
	currentCategory = event.target.value;

	loadArticles({ reset: true });
}

document.addEventListener('DOMContentLoaded', () => {
	loadCategories();
	loadArticles({ reset: true });
	setupInfiniteScroll();
  setupScrollToTopButton();
});

let timeout = null;
document.getElementById('search-input').addEventListener('input', (e) => {
	clearTimeout(timeout);
	timeout = setTimeout(() => {
		currentSearch = e.target.value.trim();
		loadArticles({ reset: true });
	}, 400);
});

document.getElementById('sort-toggle').addEventListener('click', sortToggle);

document.getElementById('category-filter').addEventListener('change', handleCategoryChange);

function setupScrollToTopButton() {
	const scrollTopBtn = document.getElementById('scroll-top-btn');
	if (!scrollTopBtn) return;

	const SHOW_THRESHOLD_PX = window.innerHeight;

	function handleScrollVisibility() {
		if (window.scrollY > SHOW_THRESHOLD_PX) {
			scrollTopBtn.hidden = false;
		} else {
			scrollTopBtn.hidden = true;
		}
	}

	window.addEventListener('scroll', handleScrollVisibility, { passive: true });
	handleScrollVisibility();

	scrollTopBtn.addEventListener('click', () => {
		window.scrollTo({ top: 0, behavior: 'smooth' });
	});
}
