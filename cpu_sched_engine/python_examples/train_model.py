"""
Example of the ML half of the project (scikit-learn), driven by data from the C++ engine.

    ./build/schedsim generate --count 20000 --balance --output dataset.csv
    python python_examples/train_model.py dataset.csv model.joblib

Needs: pip install scikit-learn pandas joblib
"""
import sys

import joblib
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier

csv_path = sys.argv[1] if len(sys.argv) > 1 else "dataset.csv"
model_path = sys.argv[2] if len(sys.argv) > 2 else "model.joblib"

df = pd.read_csv(csv_path)
X = df.drop(columns=["best_algorithm"])
y = df["best_algorithm"]
print(f"{len(df)} samples, classes:\n{y.value_counts()}\n")

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=0, stratify=y)

candidates = {
    "DecisionTree(max_depth=10)": DecisionTreeClassifier(max_depth=10, random_state=0),
    "RandomForest(200)": RandomForestClassifier(n_estimators=200, random_state=0, n_jobs=-1),
}
best_name, best_model, best_acc = None, None, 0.0
for name, model in candidates.items():
    model.fit(X_train, y_train)
    acc = accuracy_score(y_test, model.predict(X_test))
    print(f"{name:28s} test accuracy = {acc:.3f}")
    if acc > best_acc:
        best_name, best_model, best_acc = name, model, acc

print(f"\nBest: {best_name}\n")
pred = best_model.predict(X_test)
print(classification_report(y_test, pred))
print("Confusion matrix (rows = truth):")
print(pd.DataFrame(confusion_matrix(y_test, pred, labels=best_model.classes_),
                   index=best_model.classes_, columns=best_model.classes_))

if hasattr(best_model, "feature_importances_"):
    print("\nFeature importances:")
    print(pd.Series(best_model.feature_importances_, index=X.columns).sort_values(ascending=False).round(3))

# Save the feature order together with the model: it MUST match the engine's feature vector.
joblib.dump({"model": best_model, "feature_names": list(X.columns)}, model_path)
print(f"\nSaved {model_path}")
