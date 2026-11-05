# © Mikalkis
# Сборка icon.ico из векторной отрисовки (игральный кубик).
# Запуск: python make_icon.py
from PIL import Image, ImageDraw

SIZE = 256


def draw_icon() -> Image.Image:
    # Суперсэмплинг: рисуем в 4x и сжимаем, чтобы получить гладкие края.
    scale = 4
    size = SIZE * scale
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Фон: скруглённый квадрат с вертикальным градиентом индиго.
    radius = size * 55 // 128
    top, bottom = (79, 70, 229, 255), (67, 56, 202, 255)
    gradient = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    gd = ImageDraw.Draw(gradient)
    for y in range(size):
        t = y / (size - 1)
        color = tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(4))
        gd.line([(0, y), (size, y)], fill=color)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius, fill=255)
    img.paste(gradient, (0, 0), mask)

    # Кубик: белый, наклон не делаем — при 16x16 он только мутит картинку.
    cube = size * 0.68
    x0 = y0 = (size - cube) / 2
    d.rounded_rectangle(
        [x0, y0, x0 + cube, y0 + cube],
        radius=cube * 0.22,
        fill=(255, 255, 255, 255),
    )

    # Комбинация «шесть и один» не читается мелко, берём классическую пятёрку.
    pip = cube * 0.13
    pad = cube * 0.26
    centers = []
    for dx in (pad, cube / 2, cube - pad):
        for dy in (pad, cube / 2, cube - pad):
            centers.append((x0 + dx, y0 + dy))
    five = [centers[0], centers[2], centers[4], centers[6], centers[8]]
    for cx, cy in five:
        d.ellipse(
            [cx - pip / 2, cy - pip / 2, cx + pip / 2, cy + pip / 2],
            fill=(51, 46, 140, 255),
        )

    return img.resize((SIZE, SIZE), Image.LANCZOS)


def main() -> None:
    draw_icon().save(
        "icon.ico",
        format="ICO",
        sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )


if __name__ == "__main__":
    main()
