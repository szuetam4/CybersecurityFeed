let currentSort = 'desc';
let currentSearch = '';
let currentCategory = '';

async function loadArticles(category = '', sort = 'desc', search = '') {
	const feedContainer = document.getElementById('news-feed');

	const API_URL = new URL('http://localhost:8000/articles.php');
	if (category) API_URL.searchParams.append('category', category);
  if (search) API_URL.searchParams.append('q', search);
	API_URL.searchParams.append('sort', sort);

	try {
		const response = await fetch(API_URL);
		
		if(!response.ok){
			throw new Error('Błąd połączenia z API (Status: ' + response.status + ')');
		}
		
		const result = await response.json();
		const articles = result.data;
		
		feedContainer.innerHTML = '';

		if(articles.length === 0) {
			feedContainer.innerHTML = 'Brak artykułów w bazie.';
			return;
		}
    
    function isSafeUrl(url) {
      try {
        const u = new URL(url, window.location.origin);
        return ['http:', 'https:'].includes(u.protocol);
      } catch { return false; }
    }

    articles.forEach(article => {
      const card = document.createElement('article');
      card.className = 'article-card';

      const a = document.createElement('a');
    // Walidacja protokołu — blokujemy javascript:, data: itd.
      a.href = isSafeUrl(article.link) ? article.link : '#';

      const imgWrapper = document.createElement('div');
      imgWrapper.className = 'card-image-placeholder img-tech';

      const img = document.createElement('img');
      img.className = 'card-image';
      img.src = isSafeUrl(article.img_url) ? article.img_url : '';
      img.alt = 'article image';

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
      imgWrapper.append(img, badge);
      a.append(imgWrapper, content);
      card.append(a);
      feedContainer.appendChild(card);
		});
	} catch (error) {
		console.error('Krytyczny bład pobierania:', error);
		feedContainer.innerHTML = 'Nie udało się połaczyć z API, sprawdź czy serwer PHP działa w tle.';
	}
	
}

async function loadCategories(){
	const categoriesList = document.getElementById('category-filter');
	const API_URL = new URL('http://localhost:8000/categories.php');

	try{
		const response = await fetch(API_URL);

		if(!response.ok){
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

function sortToggle(){
	sortButton = document.getElementById('sort-toggle');

	if(currentSort === 'desc'){
		currentSort = 'asc';
		sortButton.textContent = 'Najnowsze';
	} else {
		currentSort = 'desc';
		sortButton.textContent = 'Najstarsze';
	}

	loadArticles(currentCategory, currentSort);
}

function handleCategoryChange(event){
	currentCategory = event.target.value;

	loadArticles(currentCategory, currentSort);
}

document.addEventListener('DOMContentLoaded', () => {
	loadCategories();
	loadArticles(currentCategory, currentSort);
});

let timeout = null;
document.getElementById('search-input').addEventListener('input', (e) => {
  clearTimeout(timeout);
  timeout = setTimeout(() => {
    currentSearch = e.target.value.trim();
    loadArticles(currentCategory, currentSort, currentSearch);
  }, 400);
});

document.getElementById('sort-toggle').addEventListener('click', sortToggle);

document.getElementById('category-filter').addEventListener('change', handleCategoryChange);
