#include "test.h"
#include "tinycthread.h"
#include <locale.h>
#include <string.h>
#define window_count 2


float panX	= 0.f;
float panY	= 0.f;
float lastX = 0.f;
float lastY = 0.f;
float zoom	= 1.0f;
bool mouseDown = false;

uint32_t test_size	= 500;	// items drawn in one run, or complexity
uint32_t iterations	= 200;	// repeat test n times
uint32_t test_width	= 400;
uint32_t test_height= 400;
bool	test_vsync	= false;
bool 	quiet		= false;//if true, don't print details and head row
bool 	first_test	= true;	//if multiple tests, dont print header row.
bool 	no_test_size= false;//several test consist of a single draw sequence without looping 'size' times
							//those test must be preceded by setting no_test_size to 'true'
int 	test_index	= 0;
int		single_test = -1;	//if not < 0, contains the index of the single test to run


static bool paused		= false;
static bool offscreen	= false;
static bool threadAware	= false;
static VkSampleCountFlags samples = VK_SAMPLE_COUNT_1_BIT;
static VkPhysicalDeviceType preferedPhysicalDeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
static char* saveToPng = NULL;

VkvgDevice device = NULL;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action != GLFW_PRESS)
		return;
	switch (key) {
	case GLFW_KEY_SPACE:
		 paused = !paused;
		break;
	case GLFW_KEY_ESCAPE :
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
#ifdef VKVG_WIRED_DEBUG
	case GLFW_KEY_F1:
		vkvg_wired_debug ^= (1U << 0);
		break;
	case GLFW_KEY_F2:
		vkvg_wired_debug ^= (1U << 1);
		break;
	case GLFW_KEY_F3:
		vkvg_wired_debug ^= (1U << 2);
		break;
#endif
	}
}
static void char_callback (GLFWwindow* window, uint32_t c){}
static void mouse_move_callback(GLFWwindow* window, double x, double y){
	if (mouseDown) {
		panX += ((float)x-lastX);
		panY += ((float)y-lastY);
	}
	lastX = (float)x;
	lastY = (float)y;
}
static void scroll_callback(GLFWwindow* window, double x, double y){
	if (y<0.f)
		zoom *= 0.5f;
	else
		zoom *= 2.0f;
}
static void mouse_button_callback(GLFWwindow* window, int but, int state, int modif){
	if (but != GLFW_MOUSE_BUTTON_1)
		return;
	if (state == GLFW_TRUE)
		mouseDown = true;
	else
		mouseDown = false;
}



void print_boxed(VkvgContext ctx, const char* text, float penX, float penY, uint32_t size, float cr, float cg, float cb)
{
	vkvg_set_font_size(ctx, size);
	vkvg_text_extents_t te = { 0 };
	vkvg_text_extents(ctx, text, &te);
	vkvg_font_extents_t fe = { 0 };
	vkvg_font_extents(ctx, &fe);

	vkvg_move_to(ctx, penX, penY);
	vkvg_rectangle(ctx, penX, penY, te.width, fe.height);
	vkvg_set_source_rgb(ctx, 0.1f, 0.2f, 0.3f);
	vkvg_fill(ctx);

	vkvg_move_to(ctx, penX, penY + fe.ascent);
	vkvg_set_source_rgb(ctx, cr, cg, cb);
	vkvg_show_text(ctx, text);
}
void print_unboxed(VkvgContext ctx, const char* text, float penX, float penY, uint32_t size) {
	vkvg_set_font_size(ctx, size);
	vkvg_move_to(ctx, penX, penY);
	vkvg_set_source_rgb(ctx, 1, 1, 1);
	vkvg_show_text(ctx, text);
}

void drawFrame(VkvgContext ctx) {

	vkvg_set_source_rgb(ctx, 0, 0, 0);
	vkvg_paint(ctx);
	//vkvg_set_source_rgb		(ctx, 1, 1, 1);

	vkvg_load_font_from_path(ctx, "d:/vkvg/tests/data/default.ttf", "droid");
	print_boxed(ctx, "1234567890", 0, 0, 16, 1, 1, 1);


	//print_boxed(ctx, "34", 1200, 1320, 23, 1, 0, 0);
	//print_boxed(ctx, "56", 1200, 1420, 24, 1, 0, 0);
	//print_boxed(ctx, "78", 1210, 1420, 25, 1, 0, 0);
	//print_boxed(ctx, "90", 1220, 1420, 26, 1, 0, 0);
	//print_boxed(ctx, "12", 1240, 1320, 27, 1, 0, 0);

	vkvg_set_source_rgba(ctx, 0, 0, 1, 0.5);
	vkvg_rectangle(ctx, 100, 100, 200, 200);
	vkvg_fill(ctx);


	////vkvg_set_source_rgba(ctx, 1, 1, 1,0.75);
	//for (uint32_t i = 0; i < 10; i++) {
	//	randomize_color(ctx);
	//	float x1 = i * 10;
	//	float y1 = (i + 200);
	//	float v = 100.f * i;

	//	vkvg_move_to(ctx, x1, y1);
	//	vkvg_line_to(ctx, x1 + v, y1);
	//	vkvg_stroke(ctx);
	//}

}



#define TRY_LOAD_DEVICE_EXT(ext) {								\
if (vkh_phyinfo_try_get_extension_properties(pi, #ext, NULL))	\
	enabledExts[enabledExtsCount++] = #ext;						\
}

static void glfw_error_callback(int error, const char* description) {
	fprintf(stderr, "vkengine: GLFW error %d: %s\n", error, description);
}

VkSampleCountFlagBits getMaxUsableSampleCount(VkSampleCountFlags counts)
{
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
	return VK_SAMPLE_COUNT_1_BIT;
}

void vkengine_dump_available_layers() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);

	VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(layerCount * sizeof(VkLayerProperties));
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	printf("Available Layers:\n");
	printf("-----------------\n");
	for (uint32_t i = 0; i < layerCount; i++) {
		printf("\t - %s\n", availableLayers[i].layerName);
	}
	printf("-----------------\n\n");
	free(availableLayers);
}
bool vkengine_try_get_phyinfo(VkhPhyInfo* phys, uint32_t phyCount, VkPhysicalDeviceType gpuType, VkhPhyInfo* phy) {
	for (uint32_t i = 0; i < phyCount; i++) {
		if (phys[i]->properties.deviceType == gpuType) {
			*phy = phys[i];
			return true;
		}
	}
	return false;
}
bool instance_extension_supported(VkExtensionProperties* instanceExtProps, uint32_t extCount, const char* instanceName) {
	for (uint32_t i = 0; i < extCount; i++) {
		if (!strcmp(instanceExtProps[i].extensionName, instanceName))
			return true;
	}
	return false;
}

typedef struct _WindowImpl
{
	GLFWwindow* window;
	VkSurfaceKHR surface;
	VkhPresenter r;
#ifdef VKVG_TEST_DIRECT_DRAW
	VkvgSurface* surfaces;
#else
	VkvgSurface surf;
#endif
} WindowImpl;

WindowImpl windows[window_count];
volatile int finishedThreadCount = 0;

int drawRectsThread(WindowImpl* window);

void perform_test_onscreen() {


	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit()) {
		perror("glfwInit failed");
		exit(-1);
	}

	if (!glfwVulkanSupported()) {
		perror("glfwVulkanSupported return false.");
		exit(-1);
	}

	const char* enabledLayers[10];
	const char* enabledExts[10];
	uint32_t enabledExtsCount = 0, enabledLayersCount = 0, phyCount = 0;

	vkh_layers_check_init();
#ifdef VKVG_USE_VALIDATION
	if (vkh_layer_is_present("VK_LAYER_KHRONOS_validation"))
		enabledLayers[enabledLayersCount++] = "VK_LAYER_KHRONOS_validation";
#endif
#ifdef VKVG_USE_MESA_OVERLAY
	if (vkh_layer_is_present("VK_LAYER_MESA_overlay"))
		enabledLayers[enabledLayersCount++] = "VK_LAYER_MESA_overlay";
#endif

#ifdef VKVG_USE_RENDERDOC
	if (vkh_layer_is_present("VK_LAYER_RENDERDOC_Capture"))
		enabledLayers[enabledLayersCount++] = "VK_LAYER_RENDERDOC_Capture";
#endif
	vkh_layers_check_release();

	uint32_t glfwReqExtsCount = 0;
	const char** gflwExts = glfwGetRequiredInstanceExtensions(&glfwReqExtsCount);

	vkvg_get_required_instance_extensions(enabledExts, &enabledExtsCount);

	for (uint32_t i = 0; i < glfwReqExtsCount; i++)
		enabledExts[i + enabledExtsCount] = gflwExts[i];

	enabledExtsCount += glfwReqExtsCount;

#ifdef VK_VERSION_1_2//VkApplicationInfo	infos;
	VkInstance inst = vkh_instance_create(1, 2, "vkvg", enabledLayersCount, enabledLayers, enabledExtsCount, enabledExts);
#else
	VkInstance inst = vkh_instance_create(1, 1, "vkvg", enabledLayersCount, enabledLayers, enabledExtsCount, enabledExts);
#endif


#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vkh_app_enable_debug_messenger(e->app
		, VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		//| VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		//| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		, NULL);
#endif

	VkhPhyInfo* phys = vkh_instance_get_phyinfos(&phyCount, inst);

	VkhPhyInfo pi = 0;
	if (!vkengine_try_get_phyinfo(phys, phyCount, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, &pi)) {
		if (!vkengine_try_get_phyinfo(phys, phyCount, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, &pi))
			pi = phys[0];
	}
	assert(pi && "No vulkan physical device found.");

	uint32_t qCount = 0;
	float qPriorities[] = { 0.0 };

	VkDeviceQueueCreateInfo pQueueInfos[3];
	if (vkh_phyinfo_create_presentable_queues(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	/*if (vkh_phyinfo_create_compute_queues		(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	if (vkh_phyinfo_create_transfer_queues		(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;*/

	enabledExtsCount = 0;

	if (vkvg_get_required_device_extensions(pi->phy, enabledExts, &enabledExtsCount) != VKVG_STATUS_SUCCESS) {
		perror("vkvg_get_required_device_extensions failed, enable log for details.\n");
		exit(-1);
	}
	TRY_LOAD_DEVICE_EXT(VK_KHR_swapchain)

	VkPhysicalDeviceFeatures enabledFeatures = { 0 };
	const void* pNext = vkvg_get_device_requirements(&enabledFeatures);

	VkDeviceCreateInfo device_info;
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.queueCreateInfoCount = qCount;
	device_info.pQueueCreateInfos = (VkDeviceQueueCreateInfo*)&pQueueInfos;
	device_info.enabledExtensionCount = enabledExtsCount;
	device_info.ppEnabledExtensionNames = enabledExts;
	device_info.pEnabledFeatures = &enabledFeatures;
	device_info.pNext = pNext;

	VkhDevice dev = vkh_device_create(inst, pi, &device_info);

	bool deferredResolve = false;

	device = vkvg_device_create_from_vk_multisample(inst,
		dev->phy,
		dev->dev,
		pi->pQueue,
		0,
		samples, deferredResolve);

	vkvg_device_set_dpy(device, 96, 96);


	if (threadAware)
		vkvg_device_set_thread_aware(device, 1);


	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

	for (int i = 0; i < window_count; i++)
	{
		windows[i].window = glfwCreateWindow((int)test_width, (int)test_height, "Window Title", NULL, NULL);

		glfwSetKeyCallback(windows[i].window, key_callback);
		glfwSetMouseButtonCallback(windows[i].window, mouse_button_callback);
		glfwSetCursorPosCallback(windows[i].window, mouse_move_callback);
		glfwSetScrollCallback(windows[i].window, scroll_callback);

		glfwCreateWindowSurface(inst, windows[i].window, NULL, &windows[i].surface);


		if (test_vsync)
		{
			windows[i].r = vkh_presenter_create(dev, (uint32_t)pi->pQueue, windows[i].surface, test_width, test_height,
				VK_FORMAT_B8G8R8A8_UNORM,
				VK_PRESENT_MODE_FIFO_KHR);
		}
		else
		{
			windows[i].r = vkh_presenter_create(dev, (uint32_t)pi->pQueue, windows[i].surface, test_width, test_height,
				VK_FORMAT_B8G8R8A8_UNORM,
				VK_PRESENT_MODE_MAILBOX_KHR);
		}
	}

	thrd_t threads[window_count];

	finishedThreadCount = 0;

	for (uint32_t i = 0; i < window_count; i++)
		thrd_create(&threads[i], drawRectsThread, &windows[i]);

	const struct timespec ts = { 1,0 };
	while (finishedThreadCount < window_count)
		thrd_sleep(&ts, NULL);

	//for (uint32_t i = 0; i < window_count; i++)
	//	drawRectsThread(&windows[i]);

	
	vkh_instance_free_phyinfos(phyCount, phys);

	vkDeviceWaitIdle(dev->dev);


	for (int i = 0; i < window_count; i++)
	{
		vkh_presenter_destroy(windows[i].r);
		vkDestroySurfaceKHR(inst, windows[i].surface, NULL);
		glfwDestroyWindow(windows[i].window);
	}


	vkvg_device_destroy(device);//销毁VkDevice的资源

	vkh_device_destroy(dev);//销毁VkDevice


	glfwTerminate();

}

int drawRectsThread(WindowImpl *window) 
{
	VkvgSurface surf;
	VkvgContext ctx;

#ifdef VKVG_TEST_DIRECT_DRAW
	window->surfaces = (VkvgSurface*)malloc(window->r->imgCount * sizeof(VkvgSurface));
	for (uint32_t i = 0; i < window->r->imgCount; i++)
		window->surfaces[i] = vkvg_surface_create_for_VkhImage(device, window->r->ScBuffers[i]);
#else
	window->surf = vkvg_surface_create(device, test_width, test_height);
	vkh_presenter_build_blit_cmd(window->r, vkvg_surface_get_vk_image(window->surf), test_width, test_height);
#endif


	for (int n = 0; !glfwWindowShouldClose(window->window) && n < iterations; ++n) {

#ifdef VKVG_TEST_DIRECT_DRAW
		if (!vkh_presenter_acquireNextImage(window->r, NULL, NULL)) {
			for (uint32_t i = 0; i < window->r->imgCount; i++)
				vkvg_surface_destroy(window->surfaces[i]);

			vkh_presenter_create_swapchain(window->r);

			for (uint32_t i = 0; i < window->r->imgCount; i++)
				window->surfaces[i] = vkvg_surface_create_for_VkhImage(device, window->r->ScBuffers[i]);
		}
		else {
			surf = window->surfaces[r->currentScBufferIndex];
			ctx = vkvg_create(surf);
			drawFrame(ctx);
			vkvg_destroy(ctx);

			//if (deferredResolve)
			//	vkvg_multisample_surface_resolve(surf);

			VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
										 .swapchainCount = 1,
										 .pSwapchains = &r->swapChain,
										 .pImageIndices = &r->currentScBufferIndex };

			vkQueuePresentKHR(r->queue, &present);
		}
#else
		surf = window->surf;
		ctx = vkvg_create(surf);
		drawFrame(ctx);
		vkvg_destroy(ctx);


		if (!vkh_presenter_draw(window->r)) {
			vkh_presenter_get_size(window->r, &test_width, &test_height);
			vkvg_surface_destroy(window->surf);
			window->surf = vkvg_surface_create(device, test_width, test_height);
			vkh_presenter_build_blit_cmd(window->r, vkvg_surface_get_vk_image(window->surf), test_width, test_height);
			vkDeviceWaitIdle(window->r->dev->dev);

		}
#endif
	}

#ifdef VKVG_TEST_DIRECT_DRAW
	for (uint32_t i = 0; i < window->r->imgCount; i++)
		vkvg_surface_destroy(window->surfaces[i]);

	free(window->surfaces);
#else
	vkvg_surface_destroy(window->surf);
#endif

	finishedThreadCount++;

	return 0;
}

int main(int argc, char* argv[]) {

	setlocale(LC_ALL, "");
	//PERFORM_TEST (simple_text, argc, argv);
	perform_test_onscreen();
	//PERFORM_TEST (single_font_and_size, argc, argv);
	//PERFORM_TEST (random_size, argc, argv);
	//PERFORM_TEST (random_font_and_size, argc, argv);
	//PERFORM_TEST (test, argc, argv);
	//PERFORM_TEST (test1, argc, argv);
	//PERFORM_TEST (test2, argc, argv);
	//PERFORM_TEST (proto_sinaitic, argc, argv);

	return 0;
}
