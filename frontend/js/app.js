let currentSort = 'desc';
let currentCategory = '';

async function loadArticles(category = '', sort = 'desc') {
	const feedContainer = document.getElementById('news-feed');

	const API_URL = new URL('http://localhost:8000/articles.php');
	if (category) API_URL.searchParams.append('category', category);
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


		articles.forEach(article => {
			const card = document.createElement('article');
			card.className = 'article-card';

			const dateObj = new Date(article.published_at);
			const formattedDate = dateObj.toLocaleDateString('pl-PL', {
				day: 'numeric',
				month: 'long',
				hour: '2-digit',
				minute: '2-digit'
			});

			card.innerHTML = `
				<a href="${article.link}" class="article-card">
					<div class="card-image-placeholder img-tech">
            <img class="card-image" src="${article.img_url}" alt="article image">
						<span class="category-badge">${article.tags}</span>
          </div>
					<div class="card-content">
						<div class="source-name">${article.source_name}</div>
						<h2 class="card-title">${article.title}</h2>
						<div class="card-footer">
							<time>${formattedDate}</time>
							<span class="read-more">Czytaj dalej &rarr;</span>
						</div>
					</div>
				</a>
			`;

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

document.getElementById('sort-toggle').addEventListener('click', sortToggle);

document.getElementById('category-filter').addEventListener('change', handleCategoryChange);
