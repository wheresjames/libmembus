#!/usr/bin/env python3
import json
import os
import sys


def fmt_num(value):
    if value >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if value >= 1_000:
        return f"{value / 1_000:.2f}K"
    return f"{value:.0f}"


def svg_escape(text):
    return (
        str(text)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


# Colours keyed by benchmark group
COLORS = {
    "memmap":  "#60a5fa",
    "memmsg":  "#34d399",
    "memcmd":  "#a78bfa",
    "memkv":   "#fb7185",
    "memvid":  "#fb923c",
    "memaud":  "#22d3ee",
    "mempkt":  "#818cf8",
    "select":  "#facc15",
    "latency": "#f472b6",
}


def bar_chart(rows, path, metric, title, unit):
    """Single-metric horizontal bar chart."""
    width        = 1100
    margin_left  = 300
    margin_right = 40
    margin_top   = 70
    bar_h        = 34
    gap          = 18
    plot_w       = width - margin_left - margin_right
    max_v = max((r[metric] for r in rows), default=1.0)
    max_v = max(max_v, 1.0)
    chart_h = margin_top + len(rows) * (bar_h + gap) + 60
    height  = max(560, chart_h)

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#0f172a"/>',
        f'<text x="{width / 2}" y="36" text-anchor="middle" font-family="Arial, sans-serif" font-size="24" font-weight="700" fill="#f8fafc">{svg_escape(title)}</text>',
        f'<line x1="{margin_left}" y1="{margin_top - 15}" x2="{margin_left + plot_w}" y2="{margin_top - 15}" stroke="#334155"/>',
    ]

    for i, r in enumerate(rows):
        y     = margin_top + i * (bar_h + gap)
        value = r[metric]
        bar_w = value / max_v * plot_w
        label = f'{r["name"]} ({r["payload"]})'
        color = COLORS.get(r.get("group", ""), "#4b5563")
        lines.append(
            f'<text x="{margin_left - 12}" y="{y + 23}" text-anchor="end" '
            f'font-family="Arial, sans-serif" font-size="14" fill="#cbd5e1">{svg_escape(label)}</text>'
        )
        lines.append(
            f'<rect x="{margin_left}" y="{y}" width="{bar_w:.2f}" height="{bar_h}" rx="4" fill="{color}"/>'
        )
        value_label = f"{fmt_num(value)} {unit}"
        if bar_w > plot_w - 130:
            lines.append(
                f'<text x="{margin_left + bar_w - 8}" y="{y + 23}" text-anchor="end" '
                f'font-family="Arial, sans-serif" font-size="14" font-weight="700" fill="#0f172a">'
                f'{svg_escape(value_label)}</text>'
            )
        else:
            lines.append(
                f'<text x="{margin_left + bar_w + 8}" y="{y + 23}" '
                f'font-family="Arial, sans-serif" font-size="14" fill="#e2e8f0">'
                f'{svg_escape(value_label)}</text>'
            )

    lines.append(
        f'<text x="{margin_left}" y="{height - 20}" font-family="Arial, sans-serif" '
        f'font-size="12" fill="#94a3b8">Generated from bench-libmembus JSON output.</text>'
    )
    lines.append("</svg>")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def latency_grouped_chart(latency_rows, path, title):
    """Grouped bar chart showing p50 and p99 side-by-side for each benchmark."""
    if not latency_rows:
        return

    n        = len(latency_rows)
    width    = 1100
    margin_l = 320
    margin_r = 40
    margin_t = 110  # extra top space for title + legend (both below y=36)
    group_h  = 80   # height per benchmark group (two bars)
    bar_h    = 28
    gap      = 6    # between p50 / p99 bars in same group
    group_gap = 24  # between groups
    plot_w   = width - margin_l - margin_r
    max_v    = max(max(r["p50_us"], r["p99_us"]) for r in latency_rows)
    max_v    = max(max_v, 1.0)
    chart_h  = margin_t + n * (group_h + group_gap) + 60
    height   = max(560, chart_h)

    color_p50 = "#34d399"
    color_p99 = "#fb7185"

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#0f172a"/>',
        f'<text x="{width / 2}" y="36" text-anchor="middle" font-family="Arial, sans-serif" font-size="24" font-weight="700" fill="#f8fafc">{svg_escape(title)}</text>',
        f'<line x1="{margin_l}" y1="{margin_t - 15}" x2="{margin_l + plot_w}" y2="{margin_t - 15}" stroke="#334155"/>',
    ]

    # Legend — placed between title and the separator line, well clear of both
    lx = margin_l
    ly = 58   # title baseline is y=36; this sits comfortably below it
    lines += [
        f'<rect x="{lx}" y="{ly}" width="16" height="16" rx="3" fill="{color_p50}"/>',
        f'<text x="{lx + 22}" y="{ly + 13}" font-family="Arial, sans-serif" font-size="13" fill="#cbd5e1">p50</text>',
        f'<rect x="{lx + 70}" y="{ly}" width="16" height="16" rx="3" fill="{color_p99}"/>',
        f'<text x="{lx + 92}" y="{ly + 13}" font-family="Arial, sans-serif" font-size="13" fill="#cbd5e1">p99</text>',
    ]

    for i, r in enumerate(latency_rows):
        base_y = margin_t + i * (group_h + group_gap)
        label  = f'{r["name"]} ({r["payload"]})'

        # Group label centred vertically over both bars
        lines.append(
            f'<text x="{margin_l - 12}" y="{base_y + bar_h + gap // 2 + 9}" text-anchor="end" '
            f'font-family="Arial, sans-serif" font-size="13" fill="#cbd5e1">{svg_escape(label)}</text>'
        )

        for j, (key, color, suffix) in enumerate([("p50_us", color_p50, "p50"), ("p99_us", color_p99, "p99")]):
            y     = base_y + j * (bar_h + gap)
            value = r[key]
            bar_w = value / max_v * plot_w
            val_label = f"{value:.0f} µs ({suffix})"
            lines.append(f'<rect x="{margin_l}" y="{y}" width="{bar_w:.2f}" height="{bar_h}" rx="4" fill="{color}"/>')
            if bar_w > plot_w - 160:
                lines.append(
                    f'<text x="{margin_l + bar_w - 8}" y="{y + 20}" text-anchor="end" '
                    f'font-family="Arial, sans-serif" font-size="13" font-weight="700" fill="#0f172a">{svg_escape(val_label)}</text>'
                )
            else:
                lines.append(
                    f'<text x="{margin_l + bar_w + 8}" y="{y + 20}" '
                    f'font-family="Arial, sans-serif" font-size="13" fill="#e2e8f0">{svg_escape(val_label)}</text>'
                )

    lines.append(
        f'<text x="{margin_l}" y="{height - 20}" font-family="Arial, sans-serif" '
        f'font-size="12" fill="#94a3b8">Lower is better. Generated from bench-libmembus JSON output.</text>'
    )
    lines.append("</svg>")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def write_summary(data, path):
    meta    = data["metadata"]
    rows    = data["results"]
    latency = data.get("latency_results", [])

    # Throughput table
    tput_lines = [
        "| Benchmark | Payload | Ops/sec | MiB/sec | ns/op |",
        "|---|---:|---:|---:|---:|",
    ]
    for r in rows:
        tput_lines.append(
            f'| {r["name"]} | {r["payload"]} | {fmt_num(r["ops_per_sec"])} | '
            f'{r["mb_per_sec"]:.1f} | {r["ns_per_op"]:.1f} |'
        )

    # Latency table
    lat_lines = []
    if latency:
        lat_lines = [
            "",
            "**Latency** (lower is better):",
            "",
            "| Benchmark | Payload | Samples | p50 µs | p95 µs | p99 µs |",
            "|---|---:|---:|---:|---:|---:|",
        ]
        for r in latency:
            lat_lines.append(
                f'| {r["name"]} | {r["payload"]} | {r["samples"]} | '
                f'{r["p50_us"]:.0f} | {r["p95_us"]:.0f} | {r["p99_us"]:.0f} |'
            )

    text = [
        "<!-- Generated by bench/plot_results.py. Do not edit by hand. -->",
        "",
        f'Benchmark duration: `{meta["duration_ms"]} ms` per case',
        "",
        f'System: `{meta.get("system", "unknown")}`',
        "",
        "**Throughput** (higher is better):",
        "",
        *tput_lines,
        *lat_lines,
        "",
    ]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(text))


def main():
    if len(sys.argv) != 3:
        print("usage: plot_results.py input.json output_dir", file=sys.stderr)
        return 2

    input_path = sys.argv[1]
    output_dir = sys.argv[2]
    os.makedirs(output_dir, exist_ok=True)

    with open(input_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    results = data["results"]
    latency = data.get("latency_results", [])

    bar_chart(results, os.path.join(output_dir, "throughput_ops.svg"),
              "ops_per_sec", "libmembus Operations Throughput", "ops/s")
    bar_chart(results, os.path.join(output_dir, "throughput_mib.svg"),
              "mb_per_sec", "libmembus Data Throughput", "MiB/s")

    if latency:
        latency_grouped_chart(latency, os.path.join(output_dir, "latency.svg"),
                              "libmembus Latency (p50 / p99)")

    write_summary(data, os.path.join(output_dir, "summary.md"))

    print(f"wrote {output_dir}/summary.md")
    print(f"wrote {output_dir}/throughput_ops.svg")
    print(f"wrote {output_dir}/throughput_mib.svg")
    if latency:
        print(f"wrote {output_dir}/latency.svg")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
