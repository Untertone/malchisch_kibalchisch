using System.Collections;
using System.Collections.Generic;
using Unigine;

public partial class DepthOfFieldUI : Component
{
	private WidgetVBox mainVBox = null;
	private WidgetComboBox qualityComboBox = null;
	private WidgetComboBox bokehComboBox = null;
	private WidgetSlider focalDistanceSlider = null;
	private WidgetLabel focalDistanceLabel = null;
	private WidgetSlider blurSlider = null;
	private WidgetLabel blurLabel = null;
	private WidgetSlider chromaticSlider = null;
	private WidgetLabel chromaticLabel = null;

	private void Init()
	{
		mainVBox = new WidgetVBox(WindowManager.MainWindow.SelfGui);
		mainVBox.SetPosition(10, 10);
		WindowManager.MainWindow.AddChild(mainVBox, Gui.ALIGN_OVERLAP);

		WidgetGridBox mainGridBox = new WidgetGridBox(2, 10, 4);
		mainVBox.AddChild(mainGridBox, Gui.ALIGN_EXPAND);

		// quality
		WidgetLabel label = new WidgetLabel("Quality:") { FontOutline = 1 };
		mainGridBox.AddChild(label, Gui.ALIGN_RIGHT | Gui.ALIGN_CENTER);

		qualityComboBox = new WidgetComboBox();
		qualityComboBox.AddItem("Low");
		qualityComboBox.AddItem("Medium");
		qualityComboBox.AddItem("High");
		qualityComboBox.AddItem("Ultra");
		mainGridBox.AddChild(qualityComboBox, Gui.ALIGN_LEFT | Gui.ALIGN_CENTER);

		qualityComboBox.EventChanged.Connect(() => Unigine.Console.SetInt("render_dof_quality", qualityComboBox.CurrentItem));

		// bokeh shape
		label = new WidgetLabel("Bokeh Shape:") { FontOutline = 1 };
		mainGridBox.AddChild(label, Gui.ALIGN_RIGHT | Gui.ALIGN_CENTER);

		bokehComboBox = new WidgetComboBox();
		bokehComboBox.AddItem("Ring");
		bokehComboBox.AddItem("Circle");
		mainGridBox.AddChild(bokehComboBox, Gui.ALIGN_LEFT | Gui.ALIGN_CENTER);

		bokehComboBox.EventChanged.Connect(() => Unigine.Console.SetInt("render_dof_bokeh_mode", bokehComboBox.CurrentItem));

		// focal distance
		label = new WidgetLabel("Focal distance:") { FontOutline = 1 };
		mainGridBox.AddChild(label, Gui.ALIGN_RIGHT | Gui.ALIGN_CENTER);

		WidgetHBox hbox = new WidgetHBox(4);

		focalDistanceSlider = new WidgetSlider(0, 10000, (int)(Render.DOFFocalDistance * 10000 / 15)) { Width = 200 };
		hbox.AddChild(focalDistanceSlider);

		focalDistanceLabel = new WidgetLabel("0") { FontOutline = 1 };
		hbox.AddChild(focalDistanceLabel);

		mainGridBox.AddChild(hbox, Gui.ALIGN_LEFT | Gui.ALIGN_CENTER);

		focalDistanceSlider.EventChanged.Connect(() =>
		{
			Render.DOFFocalDistance = focalDistanceSlider.Value * 0.0001f * 15.0f;
			UpdateLabels();
		});

		// blur
		label = new WidgetLabel("Blur:") { FontOutline = 1 };
		mainGridBox.AddChild(label, Gui.ALIGN_RIGHT | Gui.ALIGN_CENTER);

		hbox = new WidgetHBox(4);

		blurSlider = new WidgetSlider(0, 10000, (int)(Render.DOFBlur * 10000)) { Width = 200 };
		hbox.AddChild(blurSlider);

		blurLabel = new WidgetLabel("0") { FontOutline = 1 };
		hbox.AddChild(blurLabel);

		mainGridBox.AddChild(hbox, Gui.ALIGN_LEFT | Gui.ALIGN_CENTER);

		blurSlider.EventChanged.Connect(() =>
		{
			Render.DOFBlur = blurSlider.Value * 0.0001f;
			UpdateLabels();
		});

		// chromatic aberration
		label = new WidgetLabel("Chromatic aberration:") { FontOutline = 1 };
		mainGridBox.AddChild(label, Gui.ALIGN_RIGHT | Gui.ALIGN_CENTER);

		hbox = new WidgetHBox(4);

		chromaticSlider = new WidgetSlider(0, 10000, (int)(Render.DOFChromaticAberration * 10000 / 3)) { Width = 200 };
		hbox.AddChild(chromaticSlider);

		chromaticLabel = new WidgetLabel("0") { FontOutline = 1 };
		hbox.AddChild(chromaticLabel);

		mainGridBox.AddChild(hbox, Gui.ALIGN_LEFT | Gui.ALIGN_CENTER);

		chromaticSlider.EventChanged.Connect(() =>
		{
			Render.DOFChromaticAberration = chromaticSlider.Value * 0.0001f * 3.0f;
			UpdateLabels();
		});

		UpdateLabels();
	}

	private void Shutdown()
	{
		WindowManager.MainWindow?.RemoveChild(mainVBox);
	}

	private void UpdateLabels()
	{
		focalDistanceLabel.Text = (focalDistanceSlider.Value * 0.0001f * 15.0f).ToString("0.00");
		blurLabel.Text = (blurSlider.Value * 0.0001f).ToString("0.00");
		chromaticLabel.Text = (chromaticSlider.Value * 0.0001f * 3.0f).ToString("0.00");
	}
}