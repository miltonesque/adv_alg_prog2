#!/usr/bin/env python3
"""plot_results.py - turns the raw CSVs in results/ into charts and tables.

Called by reproduce.py as the last step, or by hand:

    python3 scripts/plot_results.py results

Needs pandas and matplotlib. Reads the CSVs written by benchmark_app and
explain_app, and writes:

    results/summary/   median, quartiles and spread of every timing, the
                       winner of each instance, and the karp-rabin tail table
    results/plots/     one PNG per experiment

Every timing in the plots is the median of the repetitions, with the band
showing the middle half (25th to 75th percentile). Times are per text
character so different n can be compared.
"""
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # no window, just files
import matplotlib.pyplot as plt
import pandas as pd

# same colour, marker and line style for an algorithm in every chart, so it
# can be told apart without colour too
STYLE = {
    "kmp": ("#2a78d6", "o", "-"),
    "boyer_moore": ("#eb6834", "s", "-"),
    "karp_rabin": ("#1baf7a", "^", "-"),
    "karp_rabin_fixed31": ("#eda100", "v", "--"),
    "std_find": ("#6b6a66", "D", ":"),
    # controls in the explanation tests
    "kmp_dfa": ("#6250d6", "P", "--"),
    "bm_next_position_known": ("#e87ba4", "X", "--"),
}
LABEL = {
    "kmp": "KMP",
    "boyer_moore": "Boyer-Moore",
    "karp_rabin": "Karp-Rabin (random base)",
    "karp_rabin_fixed31": "Karp-Rabin (base fixed at 31)",
    "std_find": "std::string::find",
    "kmp_dfa": "KMP as a DFA (control)",
    "bm_next_position_known": "BM, next position known (control)",
}


def q1(x):
    return x.quantile(0.25)


def q3(x):
    return x.quantile(0.75)


def tidy(ax, xlabel, ylabel, logx=True, logy=True):
    if logx:
        ax.set_xscale("log", base=2)
    if logy:
        ax.set_yscale("log")
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, which="major", color="#e4e3df", linewidth=0.6)
    ax.spines[["top", "right"]].set_visible(False)


def line(ax, df, x, algorithm, y="median", band=True):
    colour, marker, style = STYLE.get(algorithm, ("#333333", "o", "-"))
    df = df.sort_values(x)
    ax.plot(df[x], df[y], color=colour, marker=marker, linestyle=style,
            linewidth=2, markersize=6, label=LABEL.get(algorithm, algorithm))
    if band and "q1" in df and "q3" in df:
        ax.fill_between(df[x], df["q1"], df["q3"], color=colour, alpha=0.15,
                        linewidth=0)


def save(fig, path):
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print("  wrote", path)


def legend_below(fig, axes):
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=min(len(labels), 3),
               frameon=False, bbox_to_anchor=(0.5, 0.0))


def panels(summary, experiment, facet, x, xlabel, title, path, facet_name):
    data = summary[summary.experiment == experiment]
    # "sigma4" and "sigma26" are sorted by their number, other names as found
    order = {"all_match": 0, "mismatch_last": 1, "mismatch_first": 2}
    values = sorted(data[facet].unique(), key=lambda v: (
        int(v[5:]) if str(v).startswith("sigma") else order.get(v, 9)))
    fig, axes = plt.subplots(1, len(values), figsize=(4.2 * len(values), 3.8),
                             squeeze=False)
    axes = axes[0]
    for ax, value in zip(axes, values):
        part = data[data[facet] == value]
        for algorithm in STYLE:
            rows = part[part.algorithm == algorithm]
            if not rows.empty:
                line(ax, rows, x, algorithm)
        shown = value[5:] if str(value).startswith("sigma") else value
        ax.set_title(f"{facet_name} = {shown}")
        tidy(ax, xlabel, "median ns per text character")
    fig.suptitle(title)
    fig.tight_layout()
    legend_below(fig, axes)
    fig.savefig(path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print("  wrote", path)


def runtime(results, plots, summary_dir):
    raw = pd.read_csv(results / "runtime.csv")
    if (raw.verified != 1).any():
        print("  WARNING: some runs failed verification, see runtime.csv")
    keys = ["experiment", "family", "variant", "n", "m", "sigma", "algorithm"]
    summary = (raw.groupby(keys)["ns_per_char"]
               .agg(median="median", q1=q1, q3=q3, mean="mean", std="std",
                    reps="count")
               .reset_index())
    # coefficient of variation: how noisy each measurement was
    summary["cv"] = summary["std"] / summary["mean"]
    summary["iqr_over_median"] = (summary.q3 - summary.q1) / summary["median"]
    summary.to_csv(summary_dir / "runtime_summary.csv", index=False)
    print("  wrote", summary_dir / "runtime_summary.csv")

    # fastest algorithm on every instance, how far ahead it is, and whether
    # the gap is bigger than the noise (the two middle halves do not overlap)
    rows = []
    for key, group in summary.groupby(keys[:-1]):
        group = group.sort_values("median")
        best, second = group.iloc[0], group.iloc[1]
        rows.append(dict(zip(keys[:-1], key), winner=best.algorithm,
                         runner_up=second.algorithm,
                         ratio=second["median"] / best["median"],
                         clear_of_noise=bool(best.q3 < second.q1)))
    winners = pd.DataFrame(rows)
    winners.to_csv(summary_dir / "winners.csv", index=False)
    print("  wrote", summary_dir / "winners.csv")

    panels(summary, "scale_n", "variant", "n", "text length n",
           "Time per character as the text grows (m = 16)",
           plots / "scale_n.png", "alphabet")
    panels(summary, "scale_m", "variant", "m", "pattern length m",
           "Time per character as the pattern grows (n = 2^22)",
           plots / "scale_m.png", "alphabet")
    panels(summary, "repetitive", "variant", "m", "pattern length m",
           "Repetitive inputs (n = 2^18)", plots / "repetitive.png", "input")
    panels(summary, "rk_adversarial", "variant", "m", "pattern length m",
           "Text built to collide for base 31 (n = 2^18)",
           plots / "rk_adversarial.png", "input")

    data = summary[summary.experiment == "alphabet"]
    fig, ax = plt.subplots(figsize=(6.4, 4.2))
    for algorithm in STYLE:
        rows = data[data.algorithm == algorithm]
        if not rows.empty:
            line(ax, rows, "sigma", algorithm)
    tidy(ax, "alphabet size sigma", "median ns per text character")
    ax.set_title("Alphabet size (n = 2^22, m = 32)")
    ax.legend(frameon=False, fontsize=8)
    save(fig, plots / "alphabet.png")


def comparisons(results, plots, summary_dir):
    counts = pd.read_csv(results / "comparisons.csv")
    counts.to_csv(summary_dir / "comparisons.csv", index=False)
    data = counts[(counts.experiment == "scale_m")
                  & (counts.algorithm != "karp_rabin_fixed31")]
    variants = sorted(data.variant.unique(), key=lambda v: int(v[5:]))
    fig, axes = plt.subplots(1, len(variants), figsize=(4.2 * len(variants), 3.8),
                             squeeze=False)
    axes = axes[0]
    for ax, variant in zip(axes, variants):
        part = data[data.variant == variant]
        for algorithm in STYLE:
            rows = part[part.algorithm == algorithm]
            if not rows.empty:
                line(ax, rows, "m", algorithm, y="comparisons_per_char",
                     band=False)
        ax.set_title(f"alphabet = {variant[5:]}")
        tidy(ax, "pattern length m", "character comparisons per text character")
    fig.suptitle("Work done, independent of the machine (n = 2^22)")
    fig.tight_layout()
    legend_below(fig, axes)
    fig.savefig(plots / "comparisons_m.png", dpi=150, bbox_inches="tight")
    plt.close(fig)
    print("  wrote", plots / "comparisons_m.png")


def rk_tail(results, plots, summary_dir):
    tail = pd.read_csv(results / "rk_tail.csv")
    keys = ["scenario", "modulus", "n", "m", "algorithm"]
    table = (tail.groupby(keys)
             .agg(trials=("ns", "count"), median_ns=("ns", "median"),
                  p90_ns=("ns", lambda x: x.quantile(0.90)),
                  p99_ns=("ns", lambda x: x.quantile(0.99)),
                  max_ns=("ns", "max"),
                  mean_false_positives=("false_positives", "mean"),
                  trials_with_false_positive=(
                      "false_positives", lambda x: int((x.fillna(0) > 0).sum())),
                  windows=("windows", "first"),
                  bound_per_window=("bound_per_window", "first"),
                  exact_rate=("exact_rate", "first"))
             .reset_index())
    table["max_over_median"] = table.max_ns / table.median_ns
    table["false_positives_per_window"] = (table.mean_false_positives
                                           / table.windows)
    table.to_csv(summary_dir / "rk_tail_summary.csv", index=False)
    print("  wrote", summary_dir / "rk_tail_summary.csv")

    # one box per algorithm per scenario: the spread over 1000 runs
    groups = list(tail.groupby(["scenario", "modulus"]))
    fig, axes = plt.subplots(1, len(groups), figsize=(2.6 * len(groups), 4.2),
                             squeeze=False, sharey=False)
    axes = axes[0]
    for ax, ((scenario, modulus), part) in zip(axes, groups):
        order = [a for a in ("kmp", "boyer_moore", "karp_rabin")
                 if a in part.algorithm.values]
        data = [part[part.algorithm == a].ns / 1e6 for a in order]
        box = ax.boxplot(data, whis=(1, 99), showfliers=True, patch_artist=True,
                         flierprops={"markersize": 2})
        for patch, a in zip(box["boxes"], order):
            patch.set_facecolor(STYLE[a][0])
            patch.set_alpha(0.5)
        ax.set_xticks(range(1, len(order) + 1))
        ax.set_xticklabels(["KMP", "BM", "KR"])
        ax.set_yscale("log")
        ax.set_title(f"{scenario}\np = {modulus}", fontsize=8)
        ax.spines[["top", "right"]].set_visible(False)
    axes[0].set_ylabel("ms per search (1000 runs, whiskers 1st-99th pct)")
    fig.suptitle("Karp-Rabin's time over its own random choices of base")
    save(fig, plots / "rk_tail.png")

    # observed false positives per window against the (m - 1) / (p - 2) bound
    small = table[(table.scenario == "random_small_p")
                  & (table.algorithm == "karp_rabin")]
    if not small.empty:
        fig, ax = plt.subplots(figsize=(6.4, 4.2))
        ax.plot(small.modulus, small.false_positives_per_window, "o-",
                color=STYLE["karp_rabin"][0], linewidth=2,
                label="measured, mean of 1000 runs")
        ax.plot(small.modulus, small.bound_per_window, "s--", color="#6b6a66",
                linewidth=2, label="bound (m - 1) / (p - 2)")
        tidy(ax, "modulus p", "false positives per window", logx=False)
        ax.set_xscale("log")
        ax.set_title("False positives on random text (sigma = 26, m = 16)")
        ax.legend(frameon=False)
        save(fig, plots / "rk_false_positives.png")


def explanations(results, plots):
    path = results / "explain_kmp_dfa.csv"
    if path.exists():
        data = pd.read_csv(path)
        fig, ax = plt.subplots(figsize=(6.4, 4.2))
        for algorithm in data.algorithm.unique():
            rows = data[data.algorithm == algorithm].rename(
                columns={"median_ns_per_char": "median", "q1_ns_per_char": "q1",
                         "q3_ns_per_char": "q3"})
            line(ax, rows, "sigma", algorithm)
        tidy(ax, "alphabet size sigma", "median ns per text character")
        ax.set_title("T1: KMP against a DFA control (n = 2^22, m = 32)")
        ax.legend(frameon=False, fontsize=8)
        save(fig, plots / "explain_kmp_dfa.png")

    path = results / "explain_bm_dependency.csv"
    if path.exists():
        data = pd.read_csv(path)
        fig, ax = plt.subplots(figsize=(6.4, 4.2))
        for algorithm in data.algorithm.unique():
            rows = data[data.algorithm == algorithm].rename(
                columns={"median_ns_per_char": "median", "q1_ns_per_char": "q1",
                         "q3_ns_per_char": "q3"})
            line(ax, rows, "m", algorithm)
        tidy(ax, "pattern length m", "median ns per text character")
        ax.set_title("T2: text aaa...a, pattern aa...ab (n = 2^18)")
        ax.legend(frameon=False, fontsize=8)
        save(fig, plots / "explain_bm_dependency.png")

    path = results / "explain_bm_shift.csv"
    if path.exists():
        data = pd.read_csv(path)
        ms = sorted(data.m.unique())
        fig, axes = plt.subplots(1, len(ms), figsize=(4.2 * len(ms), 3.8),
                                 squeeze=False)
        axes = axes[0]
        columns = [("measured_avg_shift", "both rules (boyer_moore.cpp)", "#eb6834", "s", "-"),
                   ("badchar_only_avg_shift", "bad character only", "#2a78d6",
                    "o", "--"),
                   ("model_avg_shift", "model sigma(1-(1-1/sigma)^m)", "#6b6a66",
                    None, ":")]
        for ax, m in zip(axes, ms):
            part = data[data.m == m].sort_values("sigma")
            for column, label, colour, marker, style in columns:
                if column in part:
                    ax.plot(part.sigma, part[column], color=colour,
                            marker=marker, linestyle=style, linewidth=2,
                            markersize=6, label=label)
            ax.set_title(f"m = {m}")
            tidy(ax, "alphabet size sigma", "average shift (characters)")
        fig.suptitle("T3: how far Boyer-Moore moves per alignment (n = 2^20)")
        fig.tight_layout()
        legend_below(fig, axes)
        fig.savefig(plots / "explain_bm_shift.png", dpi=150, bbox_inches="tight")
        plt.close(fig)
        print("  wrote", plots / "explain_bm_shift.png")

    path = results / "explain_cache.csv"
    if path.exists():
        data = pd.read_csv(path)
        fig, axes = plt.subplots(1, 2, figsize=(8.4, 3.8), squeeze=False)
        axes = axes[0]
        for ax, sigma in zip(axes, sorted(data.sigma.unique())):
            part = data[data.sigma == sigma]
            pivot = part.pivot(index="algorithm", columns="layout",
                               values="median_ns_per_char")
            pivot.plot.bar(ax=ax, rot=0, color=["#2a78d6", "#eda100"])
            ax.set_title(f"alphabet = {sigma}")
            ax.set_ylabel("median ns per character")
            ax.set_xlabel("")
            ax.spines[["top", "right"]].set_visible(False)
            ax.legend(frameon=False, fontsize=8)
        fig.suptitle("T4: one 2^24 text against a 2^16 block searched 256 times")
        save(fig, plots / "explain_cache.png")


def main():
    results = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("results")
    plots = results / "plots"
    summary_dir = results / "summary"
    plots.mkdir(parents=True, exist_ok=True)
    summary_dir.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.size": 9, "axes.titlesize": 10})

    runtime(results, plots, summary_dir)
    comparisons(results, plots, summary_dir)
    rk_tail(results, plots, summary_dir)
    explanations(results, plots)


if __name__ == "__main__":
    main()
