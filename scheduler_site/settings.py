from pathlib import Path
BASE_DIR = Path(__file__).resolve().parent.parent
# SECRET_KEY = 'local-development-only-change-before-hosting'
# DEBUG = True
# ALLOWED_HOSTS = ['127.0.0.1', 'localhost']
INSTALLED_APPS = ['django.contrib.contenttypes', 'django.contrib.staticfiles', 'simulator']
MIDDLEWARE = ['django.middleware.security.SecurityMiddleware', 'whitenoise.middleware.WhiteNoiseMiddleware', 'django.middleware.common.CommonMiddleware', 'django.middleware.csrf.CsrfViewMiddleware']
STATIC_ROOT = BASE_DIR / 'staticfiles'
ROOT_URLCONF = 'scheduler_site.urls'
TEMPLATES = [{'BACKEND':'django.template.backends.django.DjangoTemplates','DIRS':[BASE_DIR/'templates'],'APP_DIRS':True,'OPTIONS':{'context_processors':['django.template.context_processors.request']}}]
WSGI_APPLICATION = 'scheduler_site.wsgi.application'
DATABASES = {'default': {'ENGINE': 'django.db.backends.sqlite3','NAME': BASE_DIR/'db.sqlite3'}}
LANGUAGE_CODE='en-us'; TIME_ZONE='Asia/Kolkata'; USE_I18N=True; USE_TZ=True
STATIC_URL='static/'
STATICFILES_DIRS=[BASE_DIR/'static']
DEFAULT_AUTO_FIELD='django.db.models.BigAutoField'

import os
SECRET_KEY = os.environ.get('SECRET_KEY', 'local-development-only-change-before-hosting')
DEBUG = os.environ.get('DEBUG', 'True').lower() == 'true'
ALLOWED_HOSTS = os.environ.get('ALLOWED_HOSTS', '127.0.0.1,localhost').split(',')