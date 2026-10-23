import json
from pathlib import Path
from django.conf import settings
def load_model():
    p=Path(settings.BASE_DIR)/'data'/'model.json'
    try:return json.loads(p.read_text(encoding='utf-8'))
    except (OSError,ValueError):return None
def predict(features):
    model=load_model()
    if not model:return None
    node=model['tree']
    while 'feature' in node:node=node['left'] if features[node['feature']]<=node['threshold'] else node['right']
    return node['class'],model['report']
