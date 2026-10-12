"""ANSI SGR styles shared by the log monitor and terminal renderer."""

from dataclasses import dataclass, replace


@dataclass(frozen=True)
class TextStyle:
    fg: int | str | None = None
    bg: int | str | None = None
    bold: bool = False
    italic: bool = False
    underline: bool = False
    reverse: bool = False


@dataclass(frozen=True)
class StyledSpan:
    text: str
    style: TextStyle


ANSI_COLORS = {
    "light": (
        "#202938", "#b92d38", "#087b3d", "#956600", "#235bb3", "#9042a2", "#087582", "#5b6677",
        "#63738a", "#d53342", "#15834b", "#a87300", "#2d6bd1", "#ae48bb", "#008896", "#24334a",
    ),
    "dark": (
        "#9aa8bc", "#ed7180", "#64cb8e", "#e8c367", "#7aabf2", "#c78de0", "#6aced9", "#d9e3ef",
        "#8294ac", "#ff8c99", "#8ee8aa", "#ffe18c", "#a0c5ff", "#e1adf4", "#8beaf3", "#ffffff",
    ),
}


def ansi_color(value, theme="light"):
    if value is None or isinstance(value, str):
        return value
    if value < 16:
        return ANSI_COLORS[theme][value]
    if value < 232:
        level = (0, 95, 135, 175, 215, 255)
        number = value - 16
        rgb = (level[number // 36], level[(number // 6) % 6], level[number % 6])
    else:
        rgb = (8 + (value - 232) * 10,) * 3
    return "#%02x%02x%02x" % rgb


def sgr_style(style, parameters):
    try:
        values = [int(value or "0") for value in parameters.split(";")]
    except ValueError:
        return style
    index = 0
    while index < len(values):
        value = values[index]
        index += 1
        if value == 0:
            style = TextStyle()
        elif value in (1, 22, 3, 23, 4, 24, 7, 27):
            name = {1: "bold", 22: "bold", 3: "italic", 23: "italic", 4: "underline", 24: "underline", 7: "reverse", 27: "reverse"}[value]
            style = replace(style, **{name: value in (1, 3, 4, 7)})
        elif 30 <= value <= 37:
            style = replace(style, fg=value - 30)
        elif 90 <= value <= 97:
            style = replace(style, fg=value - 90 + 8)
        elif 40 <= value <= 47:
            style = replace(style, bg=value - 40)
        elif 100 <= value <= 107:
            style = replace(style, bg=value - 100 + 8)
        elif value in (39, 49):
            style = replace(style, **{"fg" if value == 39 else "bg": None})
        elif value in (38, 48) and index < len(values):
            color = None
            mode = values[index]
            index += 1
            if mode == 5 and index < len(values):
                if 0 <= values[index] <= 255:
                    color = values[index]
                index += 1
            elif mode == 2 and index + 2 < len(values):
                rgb = values[index:index + 3]
                if all(0 <= component <= 255 for component in rgb):
                    color = "#%02x%02x%02x" % tuple(rgb)
                index += 3
            if color is not None:
                style = replace(style, **{"fg" if value == 38 else "bg": color})
    return style


def terminal_color(value):
    if value == "default":
        return None
    names = ("black", "red", "green", "brown", "blue", "magenta", "cyan", "white")
    if value in names:
        return names.index(value)
    if value.startswith("bright") and value[6:] in names:
        return 8 + names.index(value[6:])
    return "#" + value
