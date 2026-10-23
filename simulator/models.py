from django.db import models
class SimulationRun(models.Model):
    created_at = models.DateTimeField(auto_now_add=True)
    workload = models.JSONField()
    quantum = models.PositiveIntegerField()
    results = models.JSONField()
    recommendation = models.JSONField()
