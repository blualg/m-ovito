from __future__ import annotations

from dataclasses import dataclass
import inspect
from typing import Annotated, Any, get_args, get_origin


@dataclass(frozen=True)
class _ParameterBase:
    label: str | None = None


@dataclass(frozen=True)
class BoolParam(_ParameterBase):
    pass


@dataclass(frozen=True)
class IntParam(_ParameterBase):
    min: int | None = None
    max: int | None = None


@dataclass(frozen=True)
class FloatParam(_ParameterBase):
    min: float | None = None
    max: float | None = None


@dataclass(frozen=True)
class StringParam(_ParameterBase):
    pass


@dataclass(frozen=True)
class ChoiceParam(_ParameterBase):
    choices: tuple[Any, ...] | tuple[tuple[str, Any], ...] = ()


def modifier(cls):
    cls.__ovito_python_modifier__ = True
    return cls


def _is_modifier_class(candidate: Any) -> bool:
    return inspect.isclass(candidate) and getattr(candidate, "__ovito_python_modifier__", False)


def _normalize_choices(raw_choices):
    normalized = []
    for entry in raw_choices:
        if isinstance(entry, tuple) and len(entry) == 2:
            label, value = entry
        else:
            label = str(entry)
            value = entry
        normalized.append({"label": str(label), "value": value})
    return normalized


def _schema_kind(param_spec: _ParameterBase, annotation_type: Any) -> str:
    if isinstance(param_spec, ChoiceParam):
        return "choice"
    if isinstance(param_spec, BoolParam) or annotation_type is bool:
        return "bool"
    if isinstance(param_spec, IntParam) or annotation_type is int:
        return "int"
    if isinstance(param_spec, FloatParam) or annotation_type is float:
        return "float"
    if isinstance(param_spec, StringParam) or annotation_type is str:
        return "string"
    raise TypeError(f"Unsupported parameter annotation type: {annotation_type!r}")


def _extract_parameter_schema(modifier_class):
    annotations = inspect.get_annotations(modifier_class, eval_str=True)
    schema = []

    for name, annotation in annotations.items():
        origin = get_origin(annotation)
        parameter_spec = None
        annotation_type = annotation

        if origin is Annotated:
            args = get_args(annotation)
            annotation_type = args[0]
            for extra in args[1:]:
                if isinstance(extra, _ParameterBase):
                    parameter_spec = extra
                    break

        if parameter_spec is None:
            continue

        if not hasattr(modifier_class, name):
            raise TypeError(f"Parameter '{name}' is missing a default value.")

        default_value = getattr(modifier_class, name)
        kind = _schema_kind(parameter_spec, annotation_type)

        entry = {
            "name": name,
            "kind": kind,
            "label": parameter_spec.label or name.replace("_", " ").capitalize(),
            "default": default_value,
        }

        if isinstance(parameter_spec, (IntParam, FloatParam)):
            if parameter_spec.min is not None:
                entry["min"] = parameter_spec.min
            if parameter_spec.max is not None:
                entry["max"] = parameter_spec.max
        elif isinstance(parameter_spec, ChoiceParam):
            choices = _normalize_choices(parameter_spec.choices)
            if not choices:
                raise TypeError(f"Choice parameter '{name}' must define at least one choice.")
            if default_value not in [choice["value"] for choice in choices]:
                raise TypeError(f"Default value for choice parameter '{name}' must be one of its choices.")
            entry["choices"] = choices

        schema.append(entry)

    return schema


def _resolve_script_namespace(namespace):
    function_entry = namespace.get("modify")
    modifier_classes = [candidate for candidate in namespace.values() if _is_modifier_class(candidate)]

    if callable(function_entry) and modifier_classes:
        raise TypeError("A Python modifier script must define either a top-level modify() function or one @modifier class, not both.")

    if callable(function_entry):
        return {
            "kind": "function",
            "callable": function_entry,
            "schema": [],
        }

    if len(modifier_classes) != 1:
        raise TypeError("A class-based Python modifier script must define exactly one class decorated with @modifier.")

    modifier_class = modifier_classes[0]
    modify_method = getattr(modifier_class, "modify", None)
    if not callable(modify_method):
        raise TypeError("A @modifier class must define a callable modify(...) method.")

    return {
        "kind": "class",
        "class": modifier_class,
        "schema": _extract_parameter_schema(modifier_class),
    }


def _instantiate_modifier_class(modifier_class, values):
    instance = modifier_class()
    schema = _extract_parameter_schema(modifier_class)
    for entry in schema:
        value = values.get(entry["name"], entry["default"])
        setattr(instance, entry["name"], value)
    return instance


__all__ = [
    "modifier",
    "BoolParam",
    "IntParam",
    "FloatParam",
    "StringParam",
    "ChoiceParam",
]
