using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace AlphaPlanetarium
{
    class Isotope
    {
        public string symbol { get; set; }
        public int z { get; set; }
        public int a { get; set; }
        public double q_mev { get; set; }
        public double total_half_life_s { get; set; }
        public double ground_state_branch_fraction { get; set; }
        public double partial_half_life_s { get; set; }
        public double? parent_binding_mev { get; set; }
        public bool binding_extrapolated { get; set; }
        public string source_url { get; set; }
        public string branch_source_url { get; set; }
        public string validation_group { get; set; }
        public string holdout_reason { get; set; }
        public double? half_life_uncertainty_s { get; set; }
        public double? q_uncertainty_mev { get; set; }
        public string branch_note { get; set; }
        public string binding_note { get; set; }

        public override string ToString()
        {
            return symbol + "-" + a;
        }
    }

    class Inputs
    {
        public int z, a, l;
        public double q, r, p, h;
        public double? binding;
    }

    class Result
    {
        public double baseline, logP, outer;
        public double? qi, ren;
    }

    static class Science
    {
        const double H = 197.3269804, AMU = 931.49410242, E2 = 1.43996448;
        public static Result Calculate(Inputs x)
        {
            if (x.z < 3 || x.z > 130 || x.a < 5 || x.a > 400 || x.a - x.z < 2 || x.l < 0 || x.l > 100 || !Finite(x.q) || x.q < .01 || x.q > 100 || !Finite(x.r) || x.r < .1 || x.r > 100 || !Finite(x.p) || x.p <= 0 || x.p > 1 || !Finite(x.h) || x.h < 1e-300 || x.h > 1e30 || (x.binding.HasValue && (!Finite(x.binding.Value) || x.binding <= 0 || x.binding > 10000)))
                throw new ArgumentException("Inputs outside kernel domain. See desktop README.");
            double mu = AMU * 4 * (x.a - 4.0) / x.a, c = 2 * (x.z - 2) * E2, d = H * H * x.l * (x.l + 1) / (2 * mu), b = (c + Math.Sqrt(c * c + 4 * x.q * d)) / (2 * x.q), sum = 0;
            if (x.r < b)
            {
                double lr = Math.Log(b / x.r);
                const int n = 4096;
                for (int i = 0; i <= n; i++)
                {
                    double t = (double)i / n, r = x.r * Math.Exp(lr * t * (2 - t));
                    double v = Math.Sqrt(2 * mu * Math.Max(0, c / r + d / (r * r) - x.q)) * 2 * r * lr * (1 - t);
                    sum += (i == 0 || i == n ? 1 : (i % 2 == 1 ? 4 : 2)) * v;
                }

                sum /= 3 * n * H;
            }

            Result y = new Result();
            y.outer = b;
            y.logP = -2 * sum / Math.Log(10);
            y.baseline = Math.Log10(Math.Log(2)) - Math.Log10(x.p) - Math.Log10(x.h) - y.logP;
            if (x.z % 2 == 0 && (x.a - x.z) % 2 == 0 && x.l == 0)
            {
                double ad = x.a - 4.0, ar = 4 * ad / x.a, cp = 2 * (x.z - 2);
                y.qi = .4065 * cp * Math.Sqrt(ar / x.q) - .4311 * Math.Sqrt(ar * cp * (Math.Pow(ad, 1.0 / 3) + Math.Pow(4, 1.0 / 3))) - 20.7889;
                if (x.binding.HasValue)
                {
                    double a = x.a, z = x.z, ld = 15.5287 * a - 16.9043 * Math.Pow(a, 2.0 / 3) - 91.9686 * Math.Pow(a / 2 - z, 2) / a - .7025 * z * z / Math.Pow(a, 1.0 / 3) + 12.4439 / Math.Sqrt(a);
                    y.ren = -28.1919 - 1.0853 * Math.Pow(a, 1.0 / 6) * Math.Sqrt(z) + 1.6260 * z / Math.Sqrt(x.q) + .1078 * (x.binding.Value - ld - 2);
                }
            }

            return y;
        }

        static bool Finite(double x)
        {
            return !double.IsNaN(x) && !double.IsInfinity(x);
        }

        public static int SelfTest()
        {
            try
            {
                Inputs x = new Inputs
                {
                    z = 92,
                    a = 238,
                    q = 4.27,
                    l = 0,
                    r = 9,
                    p = .1,
                    h = 1e21
                };
                Result y = Calculate(x);
                double mu = AMU * 4 * 234 / 238, t = x.r / y.outer;
                double action = Math.Sqrt(2 * mu * x.q) * y.outer / H * (Math.Acos(Math.Sqrt(t)) - Math.Sqrt(t * (1 - t)));
                Check(Math.Abs(y.logP + 2 * action / Math.Log(10)) < 1e-7, "analytic Coulomb");
                Check(Math.Abs(y.qi.Value - 17.501939426540606) < 1e-10, "Qi kernel fixture");
                x.z = 84;
                x.a = 212;
                x.q = 8.9542;
                x.binding = 1655.7716856;
                y = Calculate(x);
                Check(Math.Abs(y.qi.Value + 6.630368823101762) < 1e-10 && Math.Abs(y.ren.Value + 6.500350233085122) < 1e-10, "Po-212 kernel fixture");
                x.l = 1;
                Result centrifugal = Calculate(x);
                Check(!centrifugal.qi.HasValue && centrifugal.logP < y.logP, "eligibility and centrifugal barrier");
                x.z = 130; x.a = 400; x.q = .01; x.r = .1; x.l = 0;
                x.binding = null;
                y = Calculate(x);
                mu = AMU * 4 * 396 / 400;
                t = x.r / y.outer;
                action = Math.Sqrt(2 * mu * x.q) * y.outer / H * (Math.Acos(Math.Sqrt(t)) - Math.Sqrt(t * (1 - t)));
                Check(Math.Abs(y.logP + 2 * action / Math.Log(10)) < 1e-6, "extreme analytic Coulomb");
                x.r = 100; x.q = 100;
                Check(Calculate(x).logP == 0, "no forbidden interval");
                Check(Math.Abs(Math.Pow(2, -1) - .5) < 1e-15, "survival");
                return 0;
            }
            catch (Exception e)
            {
                System.Diagnostics.Debug.WriteLine(e.ToString());
                return 1;
            }
        }

        static void Check(bool condition, string message)
        {
            if (!condition)
                throw new Exception(message);
        }
    }

    class WindowMain : Window
    {
        readonly Brush ink = Brush("#EDF3FC"), muted = Brush("#9EADC3"), accent = Brush("#73E0CF"), panel = Brush("#151F31");
        ComboBox isotopes, models;
        TextBox z, a, q, l, r, p, h, binding;
        TextBlock facts, estimates, status, clock;
        Slider time;
        Button play;
        Viewport3D viewport;
        PerspectiveCamera camera;
        Model3DGroup scene;
        Model3DGroup barrierRings;
        Model3DGroup populationScene;
        TextBlock legend, sceneHeading;
        bool barrierView;
        List<GeometryModel3D> particles = new List<GeometryModel3D>();
        List<double> lifetimes = new List<double>();
        DispatcherTimer timer;
        bool playing, updating;
        double yaw = .55, pitch = .35, distance = 18;
        Point drag;
        bool dragging;
        Isotope selected;
        Result result;
        static Brush Brush(string c)
        {
            return (Brush)new BrushConverter().ConvertFrom(c);
        }

        static string F(double v)
        {
            return v.ToString("0.###E+0", CultureInfo.InvariantCulture);
        }

        static string Log(double v)
        {
            return "10^" + v.ToString("0.00", CultureInfo.InvariantCulture) + " s";
        }

        static string Duration(double logSeconds)
        {
            double year = Math.Log10(31557600.0);
            if (logSeconds >= year + 9) return "10^" + (logSeconds - year).ToString("0.00") + " yr";
            if (logSeconds >= year + 6) return Math.Pow(10, logSeconds - year - 6).ToString("0.###") + " Myr";
            if (logSeconds >= year) return Math.Pow(10, logSeconds - year).ToString("0.###") + " yr";
            if (logSeconds >= 0) return Math.Pow(10, logSeconds).ToString("0.###") + " s";
            if (logSeconds >= -9) return Math.Pow(10, logSeconds + 9).ToString("0.###") + " ns";
            return Log(logSeconds);
        }

        public WindowMain()
        {
            Title = "Alpha Decay Planetarium";
            Width = 1450;
            Height = 900;
            MinWidth = 1050;
            MinHeight = 700;
            Background = Brush("#0B1220");
            Foreground = ink;
            FontFamily = new FontFamily("Segoe UI");
            Build();
            LoadCatalog();
            timer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(50)
            };
            timer.Tick += (s, e) =>
            {
                if (playing)
                {
                    time.Value = Math.Min(4, time.Value + .0125);
                    if (time.Value >= 4)
                        Toggle();
                }
            };
            timer.Start();
        }

        TextBlock Text(string value, int size, Brush color)
        {
            return new TextBlock
            {
                Text = value,
                FontSize = size,
                Foreground = color,
                TextWrapping = TextWrapping.Wrap
            };
        }

        Button Button(string label)
        {
            return new Button
            {
                Content = label,
                Background = Brush("#24354A"),
                Foreground = ink,
                BorderThickness = new Thickness(0),
                Padding = new Thickness(14, 8, 14, 8),
                Margin = new Thickness(0, 4, 8, 4),
                Cursor = Cursors.Hand
            };
        }

        Border Card(UIElement child)
        {
            return new Border
            {
                Background = panel,
                CornerRadius = new CornerRadius(12),
                Padding = new Thickness(18),
                Margin = new Thickness(0, 0, 0, 12),
                Child = child
            };
        }

        void Build()
        {
            Grid root = new Grid
            {
                Margin = new Thickness(24)
            };
            root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            root.RowDefinitions.Add(new RowDefinition());
            Content = root;
            StackPanel head = new StackPanel
            {
                Margin = new Thickness(0, 0, 0, 16)
            };
            head.Children.Add(Text("ALPHA DECAY  /  PLANETARIUM", 12, accent));
            head.Children.Add(Text("A closer look at the invisible", 28, ink));
            root.Children.Add(head);
            Grid body = new Grid();
            body.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(330) });
            body.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            body.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(330) });
            Grid.SetRow(body, 1);
            root.Children.Add(body);
            StackPanel left = new StackPanel();
            ScrollViewer ls = new ScrollViewer
            {
                Content = left,
                VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
                Margin = new Thickness(0, 0, 12, 0)
            };
            body.Children.Add(ls);
            StackPanel source = new StackPanel();
            source.Children.Add(Text("SELECT A NUCLEUS", 12, accent));
            source.Children.Add(Text("Reference isotope", 15, ink));
            isotopes = new ComboBox
            {
                Margin = new Thickness(0, 8, 0, 12),
                MinHeight = 32
            };
            source.Children.Add(isotopes);
            isotopes.SelectionChanged += (s, e) => Select();
            source.Children.Add(Text("Or edit parameters below for a custom channel.", 12, muted));
            left.Children.Add(Card(source));
            StackPanel fields = new StackPanel();
            fields.Children.Add(Text("MODEL INPUTS", 12, accent));
            z = Field(fields, "Parent Z", "84");
            a = Field(fields, "Parent A", "212");
            q = Field(fields, "Q value (MeV)", "8.9542");
            l = Field(fields, "Angular momentum ℓ", "0");
            r = Field(fields, "Boundary radius (fm)", "9");
            p = Field(fields, "Preformation", "0.1");
            h = Field(fields, "Assault frequency (s⁻¹)", "1e21");
            binding = Field(fields, "Parent binding (MeV; optional)", "");
            Button calc = Button("Calculate channel");
            calc.Click += (s, e) => Calculate();
            fields.Children.Add(calc);
            left.Children.Add(Card(fields));
            Grid center = new Grid();
            center.RowDefinitions.Add(new RowDefinition());
            center.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            Grid.SetColumn(center, 1);
            body.Children.Add(center);
            Grid visual = new Grid
            {
                Background = Brush("#101D30"),
                ClipToBounds = true
            };
            viewport = new Viewport3D();
            visual.Children.Add(viewport);
            TextBlock overlay = sceneHeading = Text("A POPULATION IN MOTION\nDrag to orbit  ·  Scroll to zoom  ·  Arrow keys rotate", 12, muted);
            overlay.Margin = new Thickness(20);
            overlay.VerticalAlignment = VerticalAlignment.Top;
            overlay.IsHitTestVisible = false;
            visual.Children.Add(overlay);
            TextBlock key = legend = Text("", 12, ink);
            SetLegend();
            key.Margin = new Thickness(20);
            key.VerticalAlignment = VerticalAlignment.Bottom;
            key.IsHitTestVisible = false;
            visual.Children.Add(key);
            center.Children.Add(visual);
            BuildScene();
            StackPanel controls = new StackPanel
            {
                Margin = new Thickness(0, 12, 0, 0)
            };
            Grid.SetRow(controls, 1);
            center.Children.Add(controls);
            StackPanel row = new StackPanel
            {
                Orientation = Orientation.Horizontal
            };
            play = Button("▶  Play");
            play.Click += (s, e) => Toggle();
            row.Children.Add(play);
            Button reset = Button("↺  Reset");
            reset.Click += (s, e) =>
            {
                playing = false;
                play.Content = "▶  Play";
                time.Value = 0;
            };
            row.Children.Add(reset);
            Button view = Button("Show barrier view");
            view.Click += (s, e) =>
            {
                barrierView = !barrierView;
                scene.Children.Remove(barrierView ? populationScene : barrierRings);
                scene.Children.Add(barrierView ? barrierRings : populationScene);
                view.Content = barrierView ? "Show population" : "Show barrier view";
                sceneHeading.Text = (barrierView ? "ONE NUCLEUS  /  BARRIER SCALE" : "A POPULATION IN MOTION") + "\nDrag to orbit  ·  Scroll to zoom  ·  Arrow keys rotate";
                SetLegend();
            };
            row.Children.Add(view);
            clock = Text("", 14, ink);
            clock.Margin = new Thickness(12, 11, 0, 0);
            row.Children.Add(clock);
            controls.Children.Add(row);
            time = new Slider
            {
                Minimum = 0,
                Maximum = 4,
                TickFrequency = .25,
                IsSnapToTickEnabled = false,
                Margin = new Thickness(0, 8, 0, 0),
                ToolTip = "Elapsed time in half-lives"
            };
            time.ValueChanged += (s, e) => UpdatePopulation();
            controls.Children.Add(time);
            status = Text("", 12, muted);
            controls.Children.Add(status);
            StackPanel right = new StackPanel();
            ScrollViewer rs = new ScrollViewer
            {
                Content = right,
                VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
                Margin = new Thickness(12, 0, 0, 0)
            };
            Grid.SetColumn(rs, 2);
            body.Children.Add(rs);
            StackPanel results = new StackPanel();
            results.Children.Add(Text("COMPARE MODELS", 12, accent));
            results.Children.Add(Text("Playback model", 15, ink));
            models = new ComboBox
            {
                Margin = new Thickness(0, 8, 0, 12),
                MinHeight = 32
            };
            models.Items.Add("Ren shell");
            models.Items.Add("Qi UDL");
            models.Items.Add("WKB baseline");
            models.SelectionChanged += (s, e) => UpdatePopulation();
            models.SelectedIndex = 0;
            results.Children.Add(models);
            estimates = Text("", 14, ink);
            results.Children.Add(estimates);
            right.Children.Add(Card(results));
            StackPanel reference = new StackPanel();
            reference.Children.Add(Text("MEASURED REFERENCE", 12, accent));
            facts = Text("", 13, ink);
            facts.Margin = new Thickness(0, 10, 0, 0);
            reference.Children.Add(facts);
            right.Children.Add(Card(reference));
            right.Children.Add(Card(Text("MODEL SCOPE\n\nWKB is a simplified external barrier with fixed preformation and assault rate. Qi and Ren apply to even-even ℓ=0 channels; parity does not establish a ground-state transition. Qi's fit covers Z=78–108. Published fits may overlap this catalog; residuals are not independent validation.\n\nOrbit with mouse drag or arrow keys; use the wheel to zoom. Playback advances in selected-model half-lives.", 12, muted)));
        }

        TextBox Field(Panel panel, string label, string value)
        {
            Grid row = new Grid
            {
                Margin = new Thickness(0, 5, 0, 0)
            };
            row.ColumnDefinitions.Add(new ColumnDefinition());
            row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(105) });
            TextBlock caption = Text(label, 12, muted);
            caption.VerticalAlignment = VerticalAlignment.Center;
            row.Children.Add(caption);
            TextBox box = new TextBox
            {
                Text = value,
                Padding = new Thickness(5),
                MinHeight = 27
            };
            System.Windows.Automation.AutomationProperties.SetName(box, label);
            Grid.SetColumn(box, 1);
            row.Children.Add(box);
            panel.Children.Add(row);
            return box;
        }

        void LoadCatalog()
        {
            using (Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream("Isotopes.json"))
            using (StreamReader reader = new StreamReader(s))
            {
                var data = new JavaScriptSerializer().Deserialize<List<Isotope>>(reader.ReadToEnd());
                foreach (var item in data)
                    isotopes.Items.Add(item);
                int index = data.FindIndex(i => i.symbol == "Po" && i.a == 212);
                isotopes.SelectedIndex = index < 0 ? 0 : index;
            }
        }

        void Select()
        {
            selected = isotopes.SelectedItem as Isotope;
            if (selected == null)
                return;
            updating = true;
            z.Text = selected.z.ToString();
            a.Text = selected.a.ToString();
            q.Text = selected.q_mev.ToString("R", CultureInfo.InvariantCulture);
            binding.Text = selected.parent_binding_mev.HasValue ? selected.parent_binding_mev.Value.ToString("R", CultureInfo.InvariantCulture) : "";
            updating = false;
            Calculate();
        }

        double D(TextBox t)
        {
            return double.Parse(t.Text, NumberStyles.Float, CultureInfo.InvariantCulture);
        }

        int I(TextBox t)
        {
            return int.Parse(t.Text, CultureInfo.InvariantCulture);
        }

        void Calculate()
        {
            if (updating)
                return;
            try
            {
                Inputs x = new Inputs
                {
                    z = I(z),
                    a = I(a),
                    q = D(q),
                    l = I(l),
                    r = D(r),
                    p = D(p),
                    h = D(h),
                    binding = String.IsNullOrWhiteSpace(binding.Text) ? (double? )null : D(binding)
                };
                result = Science.Calculate(x);
                bool match = selected != null && selected.z == x.z && selected.a == x.a && Math.Abs(selected.q_mev - x.q) < 1e-10;
                double? measured = match ? (double? )selected.partial_half_life_s : null;
                estimates.Text = "WKB baseline   " + Describe(result.baseline, measured) + "\nQi UDL              " + Describe(result.qi, measured) + "\nRen shell           " + Describe(result.ren, measured) + "\n\nOuter turning point  " + result.outer.ToString("0.##") + " fm\nlog₁₀ penetrability  " + result.logP.ToString("0.00") + (x.z < 78 || x.z > 108 ? "\nQi extrapolates beyond Z=78–108." : "");
                facts.Text = match ? selected + "  ·  Q = " + selected.q_mev + " MeV\nPartial half-life   " + Duration(Math.Log10(selected.partial_half_life_s)) + " (" + F(selected.partial_half_life_s) + " s)\nTotal half-life      " + Duration(Math.Log10(selected.total_half_life_s)) + "\nGround-state branch   " + (100 * selected.ground_state_branch_fraction).ToString("0.####") + "%\n\nHalf-life source:\n" + selected.source_url + "\n\nBranch source:\n" + selected.branch_source_url + (selected.binding_extrapolated ? "\n\nBinding energy is extrapolated." : "") : "Custom channel: no measured reference associated with these inputs.";
                if (match && selected.validation_group == "ren_holdout")
                    facts.Text += "\n\nREN HOLDOUT\n" + selected.holdout_reason + "\nQ uncertainty: " + selected.q_uncertainty_mev + " MeV\nHalf-life uncertainty: " + F(selected.half_life_uncertainty_s.GetValueOrDefault()) + " s\n" + selected.branch_note + "\n" + selected.binding_note;
                UpdateBarrier(x.r, result.outer);
                time.Value = 0;
                UpdatePopulation();
            }
            catch (Exception ex)
            {
                result = null;
                estimates.Text = "Check inputs: " + ex.Message;
                status.Text = "Calculation unavailable.";
            }
        }

        string Describe(double? v, double? measured)
        {
            if (!v.HasValue)
                return "unavailable";
            return Duration(v.Value) + (measured.HasValue ? "  (Δ " + (v.Value - Math.Log10(measured.Value)).ToString("+0.00;-0.00;0.00") + " dex)" : "");
        }

        void Toggle()
        {
            playing = !playing;
            play.Content = playing ? "❚❚  Pause" : "▶  Play";
        }

        void UpdatePopulation()
        {
            if (time == null || result == null || models == null)
                return;
            double? log = models.SelectedIndex == 0 ? result.ren : models.SelectedIndex == 1 ? result.qi : (double? )result.baseline;
            if (!log.HasValue)
            {
                status.Text = "Selected model unavailable for these inputs; choose another model.";
                playing = false;
                play.Content = "▶  Play";
                return;
            }

            double elapsed = time.Value;
            int survivors = 0;
            for (int i = 0; i < particles.Count; i++)
            {
                bool alive = lifetimes[i] > elapsed;
                if (alive)
                    survivors++;
                particles[i].Material = alive ? parentMaterial : daughterMaterial;
            }

            clock.Text = elapsed.ToString("0.00") + " half-lives";
            status.Text = survivors + " / " + particles.Count + " sampled survivors  ·  expected " + (particles.Count * Math.Pow(2, -elapsed)).ToString("0.0") + "  ·  elapsed " + (elapsed > 0 ? Duration(log.Value + Math.Log10(elapsed)) : "0 s") + "  ·  seed 42";
        }

        void SetLegend()
        {
            legend.Inlines.Clear();
            var first = new System.Windows.Documents.Run(barrierView ? "● Boundary radius" : "● Surviving parent");
            first.Foreground = Brush(barrierView ? "#F4C875" : "#73E0CF");
            legend.Inlines.Add(first);
            var second = new System.Windows.Documents.Run(barrierView ? "     ● Outer turning point" : "     ● Decayed daughter");
            second.Foreground = Brush(barrierView ? "#7DAFF5" : "#E69A75");
            legend.Inlines.Add(second);
            legend.Inlines.Add(new System.Windows.Documents.Run(barrierView ? "\nOne nucleus; radii use the same physical scale." : "\nEach sphere represents one nucleus; size is illustrative."));
        }

        Material parentMaterial = new DiffuseMaterial(Brush("#73E0CF")), daughterMaterial = new DiffuseMaterial(Brush("#E69A75"));
        void BuildScene()
        {
            scene = new Model3DGroup();
            scene.Children.Add(new AmbientLight(Color.FromRgb(85, 100, 120)));
            scene.Children.Add(new DirectionalLight(Colors.White, new Vector3D(-2, -3, -4)));
            barrierRings = new Model3DGroup();
            populationScene = new Model3DGroup();
            scene.Children.Add(populationScene);
            Random random = new Random(42);
            MeshGeometry3D mesh = Sphere();
            for (int i = 0; i < 180; i++)
            {
                double x, y, z;
                do
                {
                    x = random.NextDouble() * 2 - 1;
                    y = random.NextDouble() * 2 - 1;
                    z = random.NextDouble() * 2 - 1;
                }
                while (x * x + y * y + z * z > 1);
                GeometryModel3D m = new GeometryModel3D(mesh, parentMaterial);
                Transform3DGroup tr = new Transform3DGroup();
                tr.Children.Add(new ScaleTransform3D(.23, .23, .23));
                tr.Children.Add(new TranslateTransform3D(x * 5, y * 5, z * 5));
                m.Transform = tr;
                populationScene.Children.Add(m);
                particles.Add(m);
                lifetimes.Add(-Math.Log(1 - random.NextDouble()) / Math.Log(2));
            }

            ModelVisual3D visual = new ModelVisual3D
            {
                Content = scene
            };
            viewport.Children.Add(visual);
            camera = new PerspectiveCamera
            {
                FieldOfView = 45,
                NearPlaneDistance = .1,
                FarPlaneDistance = 100
            };
            viewport.Camera = camera;
            Camera();
            viewport.Focusable = true;
            viewport.MouseLeftButtonDown += (s, e) =>
            {
                drag = e.GetPosition(viewport);
                dragging = true;
                viewport.CaptureMouse();
                viewport.Focus();
            };
            viewport.MouseLeftButtonUp += (s, e) =>
            {
                dragging = false;
                viewport.ReleaseMouseCapture();
            };
            viewport.MouseMove += (s, e) =>
            {
                if (dragging)
                {
                    Point n = e.GetPosition(viewport);
                    yaw += (n.X - drag.X) * .01;
                    pitch = Math.Max(-1.45, Math.Min(1.45, pitch + (n.Y - drag.Y) * .01));
                    drag = n;
                    Camera();
                }
            };
            viewport.MouseWheel += (s, e) =>
            {
                distance = Math.Max(9, Math.Min(35, distance * Math.Exp(-e.Delta * .001)));
                Camera();
            };
            viewport.KeyDown += (s, e) =>
            {
                if (e.Key == Key.Left)
                    yaw -= .1;
                if (e.Key == Key.Right)
                    yaw += .1;
                if (e.Key == Key.Up)
                    pitch = Math.Max(-1.45, pitch - .1);
                if (e.Key == Key.Down)
                    pitch = Math.Min(1.45, pitch + .1);
                Camera();
            };
        }

        // These two ring radii use the same scale, preserving R / b.
        void UpdateBarrier(double radius, double outer)
        {
            barrierRings.Children.Clear();
            double scale = 5.8 / Math.Max(radius, outer);
            GeometryModel3D nucleus = new GeometryModel3D(Sphere(), new DiffuseMaterial(Brush("#D8A654")));
            nucleus.Transform = new ScaleTransform3D(radius * scale, radius * scale, radius * scale);
            barrierRings.Children.Add(nucleus);
            barrierRings.Children.Add(Ring(radius * scale, Brush("#F4C875")));
            barrierRings.Children.Add(Ring(outer * scale, Brush("#7DAFF5")));
        }

        Model3D Ring(double radius, Brush color)
        {
            const int segments = 128, sides = 8;
            const double thickness = .025;
            MeshGeometry3D mesh = new MeshGeometry3D();
            for (int i = 0; i <= segments; i++)
            {
                double angle = 2 * Math.PI * i / segments;
                for (int j = 0; j <= sides; j++)
                {
                    double cross = 2 * Math.PI * j / sides;
                    double radial = radius + thickness * Math.Cos(cross);
                    mesh.Positions.Add(new Point3D(radial * Math.Cos(angle), thickness * Math.Sin(cross), radial * Math.Sin(angle)));
                }
            }
            for (int i = 0; i < segments; i++)
                for (int j = 0; j < sides; j++)
                {
                    int a = i * (sides + 1) + j, b = a + sides + 1;
                    mesh.TriangleIndices.Add(a); mesh.TriangleIndices.Add(b); mesh.TriangleIndices.Add(a + 1);
                    mesh.TriangleIndices.Add(a + 1); mesh.TriangleIndices.Add(b); mesh.TriangleIndices.Add(b + 1);
                }
            Material material = new DiffuseMaterial(color);
            return new GeometryModel3D(mesh, material) { BackMaterial = material };
        }

        void Camera()
        {
            camera.Position = new Point3D(distance * Math.Cos(pitch) * Math.Sin(yaw), distance * Math.Sin(pitch), distance * Math.Cos(pitch) * Math.Cos(yaw));
            camera.LookDirection = new Vector3D(-camera.Position.X, -camera.Position.Y, -camera.Position.Z);
            camera.UpDirection = new Vector3D(0, 1, 0);
        }

        MeshGeometry3D Sphere()
        {
            MeshGeometry3D m = new MeshGeometry3D();
            const int lat = 12, lon = 16;
            for (int i = 0; i <= lat; i++)
            {
                double theta = Math.PI * i / lat;
                for (int j = 0; j <= lon; j++)
                {
                    double phi = 2 * Math.PI * j / lon;
                    Vector3D n = new Vector3D(Math.Sin(theta) * Math.Cos(phi), Math.Cos(theta), Math.Sin(theta) * Math.Sin(phi));
                    m.Positions.Add(new Point3D(n.X, n.Y, n.Z));
                    m.Normals.Add(n);
                }
            }

            for (int i = 0; i < lat; i++)
                for (int j = 0; j < lon; j++)
                {
                    int a = i * (lon + 1) + j, b = a + lon + 1;
                    m.TriangleIndices.Add(a);
                    m.TriangleIndices.Add(a + 1);
                    m.TriangleIndices.Add(b);
                    m.TriangleIndices.Add(a + 1);
                    m.TriangleIndices.Add(b + 1);
                    m.TriangleIndices.Add(b);
                }

            m.Freeze();
            return m;
        }
    }

    class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            if (args.Length > 0 && args[0] == "--self-test")
                return Science.SelfTest();
            Application app = new Application();
            WindowMain window = new WindowMain();
            if (args.Length == 2 && args[0] == "--render-preview")
            {
                window.Loaded += (sender, eventArgs) => window.Dispatcher.BeginInvoke(DispatcherPriority.ApplicationIdle, new Action(() =>
                {
                    try
                    {
                        RenderTargetBitmap bitmap = new RenderTargetBitmap((int)window.ActualWidth, (int)window.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                        bitmap.Render(window);
                        PngBitmapEncoder encoder = new PngBitmapEncoder();
                        encoder.Frames.Add(BitmapFrame.Create(bitmap));
                        using (FileStream output = File.Create(args[1])) encoder.Save(output);
                        app.Shutdown(0);
                    }
                    catch { app.Shutdown(1); }
                }));
            }
            return app.Run(window);
        }
    }
}
