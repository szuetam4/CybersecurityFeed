const API_URL = 'http://localhost:8000/articles.php';

async function loadArticles() {
	const feedContainer = document.getElementById('news-feed');
	
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
						<span class="category-badge">${article.tags}</span>
                                	</div>
					<div class="card-content">
						<div class="source-name">${article.source_name}</div>
						<h2 class="card-title">${article.source_name}</h2>
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

document.addEventListener('DOMContentLoaded', loadArticles());
