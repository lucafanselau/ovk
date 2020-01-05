// Ok so this is how i would image the library to look like
// very very wip

int main() {

	/* Create OVK RenderContext (atm ovk::Device and ovk::SwapChain?)*/

	ovk::ui::UIManager ui(device, swapchain);

	/* Build up UI */
	// ui::Panel is a window
	// ui::Panel is an ui::Element which is constrainable and animatable
	// adding a panel intruduces a "heavy" workload so it is adviced to do that at startup
	// each ui::Panel may have children
	
	using constraints = ovk::ui::constraints;
	ovk::ui::Panel panel = ui.build_panel()
		.add_constraint(/*x:*/ constraints::center,  /*y:*/ constraints::pixel(50), /*width*/ constraints::rel(0.1f), /*height*/ constraints::aspect(1.0f))
		.add_transition(ovk::ui::transition::alpha_fade(0.0f, 1.0f, /* timing: (seconds)*/ 0.5f))
		// The advantage of a ui::Panel is that in the add_child() method the panel will figure out constraints on its own
		// but u are free to add them urself?
		.add_child("my_button", ui.create_button("Click me!", on_button))
		.add_child("text", ui.create_text("This is some constant text"))
		.add_child("dynamic_text", ui.create_dynamic_text("This text can change later!"))
		.build();

	// u can do some manipulation at run-time
	panel.deactivate(); // will use the transition to deactivate the panel

	// u can also get a child and call the run-time function of that child
	panel["dynamic_text"].set_text("Yes now it will change!");

	// U can also use free floating ui::Elements that are not docked within a panel
	ovk::ui::Button brush_
}
