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
				<a href="${article.link}">
					<div class="article-title">
                                        	<h2 class="title">${article.title || 'Brak tytułu'}</h2>
                                	</div>
					<div class="article-tags">
						<span class="tags">${article.tags}</span>
					</div>
					<div class="article-info">
        					<h4 class="source-name">${article.source_name || 'Nieznane źródło'}
							<span class="date">(${formattedDate})</span>
						</h4>
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
