from django.urls import path
from simulator.views import home, simulate, history, health

urlpatterns = [
    path('', home, name='home'),
    path('health/', health, name='health'),
    path('api/simulate/', simulate, name='simulate'),
    path('api/history/', history, name='history'),
]
