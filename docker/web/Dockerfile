FROM php:8.3-apache

RUN apt-get update && apt-get install -y --no-install-recommends \
        libsqlite3-dev \
        sqlite3 \
    && docker-php-ext-install pdo pdo_sqlite \
    && rm -rf /var/lib/apt/lists/*

RUN a2enmod rewrite headers

COPY docker/web/apache-vhost.conf /etc/apache2/sites-available/000-default.conf
COPY docker/web/php.ini /usr/local/etc/php/conf.d/security.ini
COPY docker/web/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /var/www/html
COPY api/ ./api/
COPY database/schema.sql ./schema.sql
COPY frontend/ ./public

ENV APACHE_DOCUMENT_ROOT=/var/www/html/public
RUN sed -ri -e "s!/var/www/html!\${APACHE_DOCUMENT_ROOT}!g" /etc/apache2/apache2.conf

EXPOSE 80
ENTRYPOINT ["/entrypoint.sh"]
