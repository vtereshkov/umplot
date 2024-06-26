# UmPlot
UmPlot: A plotting library for [Umka](https://github.com/vtereshkov/umka-lang) based on [raylib](https://www.raylib.com).

## Example
```
import "umplot.um"

fn main() {
    plt := umplot::init(4)

    for i := 0; i < 4; i++ {
        plt.series[i].name = sprintf("Sine wave %d", i + 1)

        for x := 0.0; x <= 100.0; x += 1.0 {
            y := (1 + 0.5 * i) * sin(x / 10.0 + i)
            plt.series[i].add(x, y)
        }
    }

    plt.series[1].style.kind = .scatter

    plt.titles.graph = "UmPlot demo"
    plt.titles.x = "Time (seconds)"
    plt.titles.y = "Value"

    plt.plot()
}

```
![](umplot.png)

