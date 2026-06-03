// InputVisualizer — отображает на экране нажатые кнопки и движение мыши.
// Подключается к ThirdPersonMovementController для отображения состояния персонажа.
// Располагается в левом нижнем углу экрана.

using System.Text;
using Unigine;

public partial class InputVisualizer : Component
{
	#region Editor parameters

	[ShowInEditor]
	[Parameter(Group = "References", Tooltip = "Ссылка на контроллер движения (необязательно)")]
	private ThirdPersonMovementController movementController = null;

	[ShowInEditor]
	[ParameterSlider(Min = 4, Max = 24, Group = "Appearance", Tooltip = "Размер шрифта")]
	private int fontSize = 14;

	[ShowInEditor]
	[Parameter(Group = "Appearance", Tooltip = "Отступ от краёв экрана (пикселей)")]
	private int margin = 16;

	[ShowInEditor]
	[Parameter(Group = "Appearance", Tooltip = "Показывать состояние персонажа")]
	private bool showCharacterState = true;

	[ShowInEditor]
	[Parameter(Group = "Appearance", Tooltip = "Показывать клавиши движения")]
	private bool showMovementKeys = true;

	[ShowInEditor]
	[Parameter(Group = "Appearance", Tooltip = "Показывать данные мыши")]
	private bool showMouseData = true;

	#endregion

	#region Private fields

	private Gui gui = null;
	private WidgetVBox panel = null;

	// Строки состояния
	private WidgetLabel labelKeys    = null;
	private WidgetLabel labelMouse   = null;
	private WidgetLabel labelState   = null;
	private WidgetLabel labelHeader  = null;

	// Клавиши, которые мы отслеживаем
	private static readonly Input.KEY[] watchedKeys = new Input.KEY[]
	{
		Input.KEY.W, Input.KEY.A, Input.KEY.S, Input.KEY.D,
		Input.KEY.SPACE, Input.KEY.E,
		Input.KEY.ANY_CTRL, Input.KEY.ANY_SHIFT, Input.KEY.M
	};

	private static readonly string[] keyNames = new string[]
	{
		"W", "A", "S", "D",
		"Space", "E",
		"Ctrl", "Shift", "M"
	};

	#endregion

	private void Init()
	{
		gui = Gui.GetCurrent();

		// Панель — вертикальная колонка с фоном
		panel = new WidgetVBox(gui, 4, 4);
		panel.Background = 1;
		panel.BackgroundColor = new vec4(0.0f, 0.0f, 0.0f, 0.65f);
		panel.SetPadding(8, 8, 6, 6);
		panel.Lifetime = Widget.LIFETIME.WORLD;

		// Заголовок
		labelHeader = CreateLabel("<b>[ INPUT VISUALIZER ]</b>");
		labelHeader.FontSize = fontSize;
		labelHeader.FontColor = new vec4(1.0f, 0.85f, 0.2f, 1.0f);
		labelHeader.FontRich = 1;
		panel.AddChild(labelHeader, Gui.ALIGN_LEFT);

		// Клавиши
		if (showMovementKeys)
		{
			labelKeys = CreateLabel("Keys: —");
			labelKeys.FontSize = fontSize;
			labelKeys.FontColor = new vec4(0.6f, 1.0f, 0.6f, 1.0f);
			panel.AddChild(labelKeys, Gui.ALIGN_LEFT);
		}

		// Мышь
		if (showMouseData)
		{
			labelMouse = CreateLabel("Mouse: —");
			labelMouse.FontSize = fontSize;
			labelMouse.FontColor = new vec4(0.6f, 0.8f, 1.0f, 1.0f);
			panel.AddChild(labelMouse, Gui.ALIGN_LEFT);
		}

		// Состояние персонажа
		if (showCharacterState)
		{
			labelState = CreateLabel("State: —");
			labelState.FontSize = fontSize;
			labelState.FontColor = new vec4(1.0f, 0.6f, 0.6f, 1.0f);
			panel.AddChild(labelState, Gui.ALIGN_LEFT);
		}

		// Минимальная ширина панели
		panel.Width = 260;

		gui.AddChild(panel, Gui.ALIGN_OVERLAP);

		// Позиция будет обновляться в Update (зависит от размера экрана)
		UpdatePanelPosition();
	}

	private void Update()
	{
		// Обновляем позицию на случай изменения размера окна
		UpdatePanelPosition();

		// Обновляем клавиши
		if (showMovementKeys && labelKeys != null)
			labelKeys.Text = BuildKeysString();

		// Обновляем данные мыши
		if (showMouseData && labelMouse != null)
			labelMouse.Text = BuildMouseString();

		// Обновляем состояние персонажа
		if (showCharacterState && labelState != null)
			labelState.Text = BuildStateString();
	}

	private void Shutdown()
	{
		if (panel != null && gui != null)
		{
			gui.RemoveChild(panel);
			panel = null;
		}
		gui = null;
	}

	// --- Построение строк ---

	private string BuildKeysString()
	{
		var sb = new StringBuilder("Keys: ");
		bool anyPressed = false;

		for (int i = 0; i < watchedKeys.Length; i++)
		{
			if (Input.IsKeyPressed(watchedKeys[i]))
			{
				if (anyPressed) sb.Append(", ");
				sb.Append("[");
				sb.Append(keyNames[i]);
				sb.Append("]");
				anyPressed = true;
			}
		}

		if (!anyPressed)
			sb.Append("—");

		return sb.ToString();
	}

	private string BuildMouseString()
	{
		ivec2 delta = Input.MouseDeltaPosition;
		int wheel = (int)Input.MouseWheel;

		var sb = new StringBuilder();
		sb.Append("Mouse dx:");
		sb.Append(delta.x.ToString("+0;-0;0"));
		sb.Append(" dy:");
		sb.Append(delta.y.ToString("+0;-0;0"));

		if (wheel != 0)
		{
			sb.Append(" wheel:");
			sb.Append(wheel > 0 ? "▲" : "▼");
		}

		// Кнопки мыши
		bool lmb = Input.IsMouseButtonPressed(Input.MOUSE_BUTTON.LEFT);
		bool rmb = Input.IsMouseButtonPressed(Input.MOUSE_BUTTON.RIGHT);
		bool mmb = Input.IsMouseButtonPressed(Input.MOUSE_BUTTON.MIDDLE);

		if (lmb || rmb || mmb)
		{
			sb.Append(" [");
			if (lmb) sb.Append("LMB ");
			if (rmb) sb.Append("RMB ");
			if (mmb) sb.Append("MMB");
			sb.Append("]");
		}

		return sb.ToString();
	}

	private string BuildStateString()
	{
		if (movementController == null || !movementController.IsInitialized)
			return "State: (no controller)";

		var state = movementController.State;
		float speed = movementController.Speed;

		return string.Format("State: {0}  spd:{1:F1}", state, speed);
	}

	// --- Вспомогательные методы ---

	private WidgetLabel CreateLabel(string text)
	{
		var lbl = new WidgetLabel(gui, text);
		lbl.FontOutline = 1;
		return lbl;
	}

	private void UpdatePanelPosition()
	{
		if (panel == null || gui == null)
			return;

		// Высота панели неизвестна точно до рендера, используем приближение:
		// каждая строка ~(fontSize + 6), плюс padding + заголовок
		int lineCount = 1; // заголовок
		if (showMovementKeys) lineCount++;
		if (showMouseData) lineCount++;
		if (showCharacterState) lineCount++;

		int approxHeight = lineCount * (fontSize + 8) + 20;

		int screenH = gui.Height;
		int posX = margin;
		int posY = screenH - approxHeight - margin;

		panel.PositionX = posX;
		panel.PositionY = posY;
	}
}
