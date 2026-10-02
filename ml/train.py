"""Train and export a fully-int8 four-class path-direction classifier."""
from __future__ import annotations
import argparse, json
from pathlib import Path
import numpy as np
import tensorflow as tf
from sklearn.metrics import classification_report, confusion_matrix

SEED = 42
LABELS = ["blocked", "forward", "left", "right"]  # alphabetic folder order
SIZE = (96, 96)


def datasets(root: str, batch: int):
    common = dict(directory=root, validation_split=0.30, seed=SEED,
                  image_size=SIZE, batch_size=batch, label_mode="categorical")
    train = tf.keras.utils.image_dataset_from_directory(subset="training", **common)
    held = tf.keras.utils.image_dataset_from_directory(subset="validation", **common)
    n = tf.data.experimental.cardinality(held)
    val = held.take(n // 2)
    test = held.skip(n // 2)
    return train, val, test


def make_model():
    aug = tf.keras.Sequential([
        tf.keras.layers.RandomBrightness(0.18),
        tf.keras.layers.RandomContrast(0.15),
        # Do not horizontally flip: it exchanges left and right labels.
    ])
    return tf.keras.Sequential([
        tf.keras.layers.Input((*SIZE, 3)), aug,
        tf.keras.layers.Rescaling(1 / 255.0),
        tf.keras.layers.Conv2D(8, 3, strides=2, padding="same", activation="relu"),
        tf.keras.layers.DepthwiseConv2D(3, strides=2, padding="same", activation="relu"),
        tf.keras.layers.Conv2D(16, 1, activation="relu"),
        tf.keras.layers.DepthwiseConv2D(3, strides=2, padding="same", activation="relu"),
        tf.keras.layers.Conv2D(32, 1, activation="relu"),
        tf.keras.layers.DepthwiseConv2D(3, strides=2, padding="same", activation="relu"),
        tf.keras.layers.Conv2D(48, 1, activation="relu"),
        tf.keras.layers.GlobalAveragePooling2D(),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(4, activation="softmax"),
    ])


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--data", default="ml/dataset")
    p.add_argument("--epochs", type=int, default=35)
    p.add_argument("--batch", type=int, default=32)
    a = p.parse_args()
    np.random.seed(SEED); tf.random.set_seed(SEED)
    train, val, test = datasets(a.data, a.batch)
    assert train.class_names == LABELS, f"Expected folders {LABELS}, got {train.class_names}"
    autotune = tf.data.AUTOTUNE
    train = train.cache().shuffle(1000, seed=SEED).prefetch(autotune)
    val = val.cache().prefetch(autotune); test = test.cache().prefetch(autotune)
    model = make_model()
    model.compile(optimizer=tf.keras.optimizers.Adam(1e-3), loss="categorical_crossentropy", metrics=["accuracy"])
    out = Path("ml/models"); out.mkdir(parents=True, exist_ok=True)
    callbacks = [
        tf.keras.callbacks.EarlyStopping(patience=7, restore_best_weights=True),
        tf.keras.callbacks.ModelCheckpoint(out / "navigation.keras", save_best_only=True),
        tf.keras.callbacks.ReduceLROnPlateau(patience=3, factor=0.4),
    ]
    history = model.fit(train, validation_data=val, epochs=a.epochs, callbacks=callbacks)
    truth, pred = [], []
    for x, y in test:
        truth.extend(np.argmax(y.numpy(), 1)); pred.extend(np.argmax(model.predict(x, verbose=0), 1))
    report = classification_report(truth, pred, target_names=LABELS, output_dict=True, zero_division=0)
    metrics = {"labels": LABELS, "confusion_matrix": confusion_matrix(truth, pred).tolist(), "report": report,
               "history": history.history}
    (out / "metrics.json").write_text(json.dumps(metrics, indent=2))

    # Representative real camera frames are essential for accurate integer calibration.
    def representative():
        for x, _ in train.unbatch().batch(1).take(200):
            yield [tf.cast(x, tf.float32)]
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.uint8
    converter.inference_output_type = tf.uint8
    (out / "navigation_int8.tflite").write_bytes(converter.convert())
    print("Saved model and test metrics to", out)

if __name__ == "__main__": main()
