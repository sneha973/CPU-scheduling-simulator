from django.db import migrations, models
class Migration(migrations.Migration):
    initial=True
    dependencies=[]
    operations=[migrations.CreateModel(name='SimulationRun',fields=[('id',models.BigAutoField(auto_created=True,primary_key=True,serialize=False,verbose_name='ID')),('created_at',models.DateTimeField(auto_now_add=True)),('workload',models.JSONField()),('quantum',models.PositiveIntegerField()),('results',models.JSONField()),('recommendation',models.JSONField())])]
