from django.urls import path
from simulator.views import home, simulate, history
urlpatterns = [path('', home, name='home'), path('api/simulate/', simulate, name='simulate'), path('api/history/', history, name='history')]
