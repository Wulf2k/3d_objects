//
// 3d_objects.cpp - 3D Objects
//
// Copyright 2007 by Ken Paulson
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the Drunken Hyena License.  If a copy of the license was
// not included with this software, you may get a copy from:
// http://www.drunkenhyena.com/docs/DHLicense.txt
//
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <D3DX9.h>
#include <dinput.h>
#include "../common/dhWindow.h"
#include "../common/dhD3D.h"
#include "../common/dhUtility.h"
#include "../Common/dhUserPrefsDialog.h"

// This is causes the required libraries to be linked in, the same thing can be accomplished by
// adding it to your compiler's link list (Project->Settings->Link in VC++),
// but I prefer this method.
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"dxerr9.lib")
#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

#ifdef _DEBUG
  #pragma comment(lib,"d3dx9d.lib")
#else
  #pragma comment(lib,"d3dx9.lib")
#endif


// Forward declarations for all of our functions, see their definitions for more detail
LRESULT CALLBACK default_window_proc(HWND p_hwnd,UINT p_msg,WPARAM p_wparam,LPARAM p_lparam);
HRESULT init_scene(void);
void kill_scene(void);
HRESULT render(void);
void set_device_states(void);
void init_matrices(void);
HRESULT init_lists(void);
void draw_pyramid(void);
void draw_cube(void);
void move_cam(void);
bool InitInput(HWND hWnd);
bool UpdateInput(void);
bool ReleaseInput(void);

// The name of our application.  Used for window and MessageBox titles and error reporting
const char *g_app_name = "3D Objects";

// Our screen/window sizes and bit depth.  A better app would allow the user to choose the
// sizes.  I'll do that in a later tutorial, for now this is good enough.
const int g_width = 800;
const int g_height = 480;
const int g_depth = 16; //16-bit colour

// Our global flag to track whether we should quit or not.  When it becomes true, we clean
// up and exit.
bool g_app_done = false;

// Our main Direct3D interface, it doesn't do much on its own, but all the more commonly
// used interfaces are created by it.  It's the first D3D object you create, and the last
// one you release.
IDirect3D9 *g_D3D = NULL;

// The D3DDevice is your main rendering interface.  It represents the display and all of its
// capabilities.  When you create, modify, or render any type of resource, you will likely
// do it through this interface.
IDirect3DDevice9 *g_d3d_device = NULL;

//Our presentation parameters.  They get set in our call to dhInitDevice, and we need them
//in case we need to reset our application.
D3DPRESENT_PARAMETERS g_pp;

struct tri_vertex
{
    float x, y, z;  // The transformed(screen space) position for the vertex.
    DWORD colour;        // The vertex colour.
};

const DWORD tri_fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

IDirect3DVertexBuffer9 *g_list_vb = NULL;

const int g_pyramid_count = 4 * 1; //4 sides, each side made up of 1 triangle
const int g_cube_count = 6 * 2; //6 faces, each face is 2 triangles


LPDIRECTINPUT8         lpdi;
LPDIRECTINPUTDEVICE8   m_keyboard;
LPDIRECTINPUTDEVICE8   m_mouse;

UCHAR keystate[256];
DIMOUSESTATE mouse_state;


float x = 0, y = 0, z = 0;

D3DXMATRIX view_matrix;
D3DXMATRIX projection_matrix;
D3DXVECTOR3 eye_vector;
D3DXVECTOR3 lookat_vector;
D3DXVECTOR3 up_vector;
D3DXMATRIX world_matrix;
float aspect;

//******************************************************************************************
// Function:WinMain
// Whazzit:The entry point of our application
//******************************************************************************************
int APIENTRY WinMain(HINSTANCE ,HINSTANCE ,LPSTR ,int ){
bool fullscreen;
HWND window = NULL;
D3DFORMAT format;
HRESULT hr;
dhUserPrefs user_prefs(g_app_name);

   dhLogErase();  //Erase the log file to start fresh

	dhLog("Starting application\n");

   // Prompt the user for their preferences
   if (!user_prefs.QueryUser())
	{
      dhLog("Exiting\n");
      return 0;
   }

   fullscreen = user_prefs.GetFullscreen();


   // Build our window.
   hr=dhInitWindow(fullscreen, g_app_name, g_width, g_height, default_window_proc, &window);
   if(FAILED(hr))
	{
      dhLog("Failed to create Window",hr);
      return 0;
   }

   //Build the D3D object
   hr=dhInitD3D(&g_D3D);
   if(FAILED(hr))
	{
      dhKillWindow(&window);
      dhLog("Failed to create D3D",hr);
      return 0;
   }

   //Find a good display/pixel format
   hr=dhGetFormat(g_D3D,fullscreen,g_depth,&format);
   if(FAILED(hr))
	{
      dhKillWindow(&window);
      dhLog("Failed to get a display format",hr);
      return 0;
   }

   DWORD adapter = user_prefs.GetAdapter();
   D3DDEVTYPE dev_type = user_prefs.GetDeviceType();

   //Initialize our PresentParameters
   dhInitPresentParameters(fullscreen,window,g_width,g_height,format,D3DFMT_UNKNOWN,&g_pp);

   //Create our device
   hr=dhInitDevice(g_D3D,adapter,dev_type,window,&g_pp,&g_d3d_device);
   if(FAILED(hr))
	{
      dhKillD3D(&g_D3D,&g_d3d_device);
      dhKillWindow(&window);
      dhLog("Failed to create the device",hr);
      return 0;
   }

   //One-time preparation of objects and other stuff required for rendering
   if(FAILED(init_scene()))
	{
      kill_scene();
      dhKillD3D(&g_D3D,&g_d3d_device);
      dhKillWindow(&window);
      return 0;
   }

   InitInput(window);

   //Loop until the user aborts (closes the window,presses the left mouse button or hits a key)
   while(!g_app_done)
	{
      
      dhMessagePump();   //Check for window messages
	  UpdateInput();

      hr = g_d3d_device->TestCooperativeLevel();

      if(SUCCEEDED(hr))
		{
         hr = render();   //Draw our incredibly cool graphics
      }

      //Our device is lost
      if(hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET){

         dhHandleLostDevice(g_d3d_device,&g_pp,hr);

      }
		else if(FAILED(hr)) //Any other error
		{ 
         g_app_done=true;
         dhLog("Error rendering",hr);
      }

   }

   //Free all of our objects and other resources
   kill_scene();

   //Clean up all of our Direct3D objects
   dhKillD3D(&g_D3D,&g_d3d_device);

   ReleaseInput();

   //Close down our window
   dhKillWindow(&window);

	dhLog("Exiting normally\n");

   //Exit happily
   return 0;
}


bool InitInput(HWND hWnd)
{
	if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
		IID_IDirectInput8, (void**)&lpdi, NULL)))
		return false;

	// initialize the keyboard
	if (FAILED(lpdi->CreateDevice(GUID_SysKeyboard, &m_keyboard, NULL)))
		return false;
	if (FAILED(m_keyboard->SetDataFormat(&c_dfDIKeyboard)))
		return false;
	if (FAILED(m_keyboard->SetCooperativeLevel(hWnd, DISCL_BACKGROUND |
		DISCL_NONEXCLUSIVE)))
		return false;
	if (FAILED(m_keyboard->Acquire()))
		return false;

	// initialize the mouse
	if (FAILED(lpdi->CreateDevice(GUID_SysMouse, &m_mouse, NULL)))
		return false;
	if (FAILED(m_mouse->SetCooperativeLevel(hWnd, DISCL_BACKGROUND |
		DISCL_NONEXCLUSIVE)))
		return false;
	if (FAILED(m_mouse->SetDataFormat(&c_dfDIMouse)))
		return false;
	if (FAILED(m_mouse->Acquire()))
		return false;

	return true;
}
bool UpdateInput(void)
{
	//if (FAILED(m_keyboard->GetDeviceState(sizeof(UCHAR[256]), (LPVOID)keystate)))
		//return false;
	if (FAILED(m_mouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mouse_state)))
		return false;


	x = x + (mouse_state.lX/10.0f);
	y = y - (mouse_state.lY/10.0f);


	return true;
}
bool ReleaseInput(void)
{
	m_mouse->Unacquire();
	m_mouse->Release();
	m_mouse = NULL;

	m_keyboard->Unacquire();
	m_keyboard->Release();
	m_keyboard = NULL;

	lpdi->Release();
	lpdi = NULL;

	return true;
}
//******************************************************************************************
// Function:InitVolatileResources
// Whazzit:Prepare any objects that will not survive a device Reset.  These are initialized
//         separately so they can easily be recreated when we Reset our device.
//******************************************************************************************
void InitVolatileResources(void)
{

   set_device_states();

   init_matrices();

}
//******************************************************************************************
// Function:FreeVolatileResources
// Whazzit:Free any of our resources that need to be freed so that we can Reset our device,
//         also used to free these resources at the end of the program run.
//******************************************************************************************
void FreeVolatileResources(void)
{

   //This sample has no resources that need to be freed here.

}
//******************************************************************************************
// Function:init_scene
// Whazzit:Prepare any objects required for rendering.
//******************************************************************************************
HRESULT init_scene(void)
{
HRESULT hr=D3D_OK;

   InitVolatileResources();
   
   hr = init_lists();

   return hr;

}
//******************************************************************************************
// Function:kill_scene
// Whazzit:Clean up any objects we required for rendering.
//******************************************************************************************
void kill_scene(void){

   if(g_list_vb)
	{
      g_list_vb->Release();
      g_list_vb = NULL;
   }

   FreeVolatileResources();

}
//******************************************************************************************
// Function:set_device_states
// Whazzit:Sets up required RenderStates, TextureStageStates and SamplerStates
//******************************************************************************************
void set_device_states(void){

   //Turn off D3D lighting, since we are providing our own vertex colours
   //Transformed Vertices are not lit by D3D but by their vertex colours by default.
   //Untransformed vertices by default are lit by D3D, since we haven't added any
   //lighting, we wouldn't see anything if we didn't do this.
   g_d3d_device->SetRenderState(D3DRS_LIGHTING,FALSE);
   


   float Start = 7.0f,    // Linear fog distances
	End = 8.5f;

   g_d3d_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
   g_d3d_device->SetRenderState(D3DRS_FOGCOLOR, 0x00FFFFFF);
   g_d3d_device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
   g_d3d_device->SetRenderState(D3DRS_FOGSTART, *(DWORD *)(&Start));
   g_d3d_device->SetRenderState(D3DRS_FOGEND, *(DWORD *)(&End));
   // create material
   D3DMATERIAL9 mtrl;
   ZeroMemory(&mtrl, sizeof(mtrl));
   mtrl.Ambient.r = 0.75f;
   mtrl.Ambient.g = 0.0f;
   mtrl.Ambient.b = 0.0f;
   mtrl.Ambient.a = 0.0f;
   g_d3d_device->SetMaterial(&mtrl);
   g_d3d_device->SetRenderState(D3DRS_AMBIENT, 0x007fbfbf);


   g_d3d_device->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);      //Default culling
   //g_d3d_device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);   //No culling
   //g_d3d_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
}
//******************************************************************************************
// Function:init_matrices
// Whazzit:One-time preparation of our View and Projection matrices
//******************************************************************************************
void init_matrices(void){


   //Here we build our View Matrix, think of it as our camera.

   //First we specify that our viewpoint is 8 units back on the Z-axis
   eye_vector=D3DXVECTOR3( 0.0f, 0.0f,-8.0f );

   //We are looking towards the origin
   lookat_vector=D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

   //The "up" direction is the positive direction on the y-axis
   up_vector=D3DXVECTOR3(0.0f,1.0f,0.0f);

   D3DXMatrixLookAtLH(&view_matrix,&eye_vector,
                                   &lookat_vector,
                                   &up_vector);

   //Since our 'camera' will never move, we can set this once at the
   //beginning and never worry about it again
   g_d3d_device->SetTransform(D3DTS_VIEW,&view_matrix);

   aspect=((float)g_width / (float)g_height);

   D3DXMatrixPerspectiveFovLH(&projection_matrix, //Result Matrix
                              D3DX_PI/4,          //Field of View, in radians.
                              aspect,             //Aspect ratio
                              1.0f,               //Near view plane
                              100.0f );           //Far view plane

   //Our Projection matrix won't change either, so we set it now and never touch
   //it again.
   g_d3d_device->SetTransform(D3DTS_PROJECTION, &projection_matrix);

}

//******************************************************************************************
// Function:render
// Whazzit:Clears the screen, renders our scene and then presents the results.
//******************************************************************************************
HRESULT render(void){
HRESULT hr;

	move_cam();
   //Clear the buffer to our new colour.
   hr=g_d3d_device->Clear(0,  //Number of rectangles to clear, we're clearing everything so set it to 0
                          NULL, //Pointer to the rectangles to clear, NULL to clear whole display
                          D3DCLEAR_TARGET,   //What to clear.  We don't have a Z Buffer or Stencil Buffer
                          0x00000000, //Colour to clear to (AARRGGBB)
                          1.0f,  //Value to clear ZBuffer to, doesn't matter since we don't have one
                          0 );   //Stencil clear value, again, we don't have one, this value doesn't matter
   if(FAILED(hr))
	{
      return hr;
   }

   //Notify the device that we're ready to render
   hr=g_d3d_device->BeginScene();
   if(FAILED(hr))
	{
      return hr;
   }


   g_d3d_device->SetFVF(tri_fvf);


   //Bind our Vertex Buffer
   g_d3d_device->SetStreamSource(0,                   //StreamNumber
                                 g_list_vb,           //StreamData
                                 0,                   //OffsetInBytes
                                 sizeof(tri_vertex)); //Stride

   draw_pyramid();
   
   draw_cube();

   //Notify the device that we're finished rendering for this frame
   g_d3d_device->EndScene();

   //Show the results
   hr=g_d3d_device->Present(NULL,  //Source rectangle to display, NULL for all of it
                            NULL,  //Destination rectangle, NULL to fill whole display
                            NULL,  //Target window, if NULL uses device window set in CreateDevice
                            NULL );//Unused parameter, set it to NULL

   return hr;
}
//******************************************************************************************
// Function:draw_pyramid
// Whazzit:Calculates the new rotation and position, then renders the pyramid
//******************************************************************************************
void draw_pyramid(void){
D3DXMATRIX rot_matrix;
D3DXMATRIX trans_matrix;
D3DXMATRIX world_matrix;
static float rot_triangle=0.0f;


   D3DXMatrixRotationY(&rot_matrix,rot_triangle);  //Rotate the pyramid
   D3DXMatrixTranslation(&trans_matrix,-2.0f,0,0); //Shift it 2 units to the left
   D3DXMatrixMultiply(&world_matrix,&rot_matrix,&trans_matrix);

   g_d3d_device->SetTransform(D3DTS_WORLD,&world_matrix);

   //Render from our Vertex Buffer
   g_d3d_device->DrawPrimitive(D3DPT_TRIANGLELIST, //PrimitiveType
                               0,                  //StartVertex
                               g_pyramid_count);   //PrimitiveCount

   rot_triangle+=0.007f;
   if(rot_triangle > D3DX_PI*2)
	{  
      rot_triangle-=D3DX_PI*2;
   }

}
//******************************************************************************************
// Function:draw_cube
// Whazzit:Calculates the new rotation and position, then renders the cube
//******************************************************************************************
void draw_cube(void){
D3DXMATRIX rot_matrix;
D3DXMATRIX trans_matrix;
D3DXMATRIX world_matrix;
static float rot_cube=0.0f;
int start_vertex;


   //Offset past the pyramid, offset is given as the number of vertices to be skipped
   start_vertex=g_pyramid_count * 3;


   D3DXMatrixRotationYawPitchRoll(&rot_matrix,0.0f,rot_cube,rot_cube);  //Rotate the cube
   D3DXMatrixTranslation(&trans_matrix,2.0f,0,0); //Shift it 2 units to the right
   D3DXMatrixMultiply(&world_matrix,&rot_matrix,&trans_matrix);   //Rot & Trans

   g_d3d_device->SetTransform(D3DTS_WORLD,&world_matrix);

   //Render from our Vertex Buffer
   g_d3d_device->DrawPrimitive(D3DPT_TRIANGLELIST, //PrimitiveType
                               start_vertex,       //StartVertex
                               g_cube_count);      //PrimitiveCount

   rot_cube+=0.006f;
   if(rot_cube > D3DX_PI*2)
	{  
      rot_cube-=D3DX_PI*2;
   }

}
//******************************************************************************************
// Function:init_lists
// Whazzit:Initialize the data in our Vertex Buffer
//******************************************************************************************
HRESULT init_lists(void){
tri_vertex data[]={
   //Pyramid vertices
   {-1.0f,-1.0f,-1.0f,0x0000FF00},{ 0.0f, 1.0f, 0.0f,0x00FF0000},{ 1.0f,-1.0f,-1.0f,0x000000FF},
   { 1.0f,-1.0f,-1.0f,0x000000FF},{ 0.0f, 1.0f, 0.0f,0x00FF0000},{ 1.0f,-1.0f, 1.0f,0x0000FF00},
   { 1.0f,-1.0f, 1.0f,0x0000FF00},{ 0.0f, 1.0f, 0.0f,0x00FF0000},{-1.0f,-1.0f, 1.0f,0x000000FF},
   {-1.0f,-1.0f, 1.0f,0x000000FF},{ 0.0f, 1.0f, 0.0f,0x00FF0000},{-1.0f,-1.0f,-1.0f,0x0000FF00},
   //Cube vertices
      //Front face
   {-1.0f,-1.0f,-1.0f,0xFF0000FF},{-1.0f, 1.0f,-1.0f,0xFF0000FF},{ 1.0f, 1.0f,-1.0f,0xFF0000FF},
   { 1.0f, 1.0f,-1.0f,0xFF0000FF},{ 1.0f,-1.0f,-1.0f,0xFF0000FF},{-1.0f,-1.0f,-1.0f,0xFF0000FF},
      //Back face
   { 1.0f,-1.0f, 1.0f,0xFF0000FF},{ 1.0f, 1.0f, 1.0f,0xFF0000FF},{-1.0f, 1.0f, 1.0f,0xFF0000FF},
   {-1.0f, 1.0f, 1.0f,0xFF0000FF},{-1.0f,-1.0f, 1.0f,0xFF0000FF},{ 1.0f,-1.0f, 1.0f,0xFF0000FF},
      //Top face
   {-1.0f, 1.0f,-1.0f,0xFFFF0000},{-1.0f, 1.0f, 1.0f,0xFFFF0000},{ 1.0f, 1.0f, 1.0f,0xFFFF0000},
   { 1.0f, 1.0f, 1.0f,0xFFFF0000},{ 1.0f, 1.0f,-1.0f,0xFFFF0000},{-1.0f, 1.0f,-1.0f,0xFFFF0000},
      //Bottom face
   { 1.0f,-1.0f,-1.0f,0xFFFF0000},{ 1.0f,-1.0f, 1.0f,0xFFFF0000},{-1.0f,-1.0f, 1.0f,0xFFFF0000},
   {-1.0f,-1.0f, 1.0f,0xFFFF0000},{-1.0f,-1.0f,-1.0f,0xFFFF0000},{ 1.0f,-1.0f,-1.0f,0xFFFF0000},
      //Left face
   {-1.0f,-1.0f, 1.0f,0xFF00FF00},{-1.0f, 1.0f, 1.0f,0xFF00FF00},{-1.0f, 1.0f,-1.0f,0xFF00FF00},
   {-1.0f, 1.0f,-1.0f,0xFF00FF00},{-1.0f,-1.0f,-1.0f,0xFF00FF00},{-1.0f,-1.0f, 1.0f,0xFF00FF00},
      //Right face
   { 1.0f,-1.0f,-1.0f,0xFF00FF00},{ 1.0f, 1.0f,-1.0f,0xFF00FF00},{ 1.0f, 1.0f, 1.0f,0xFF00FF00},
   { 1.0f, 1.0f, 1.0f,0xFF00FF00},{ 1.0f,-1.0f, 1.0f,0xFF00FF00},{ 1.0f,-1.0f,-1.0f,0xFF00FF00},

};
void *vb_vertices;
HRESULT hr;

   hr=g_d3d_device->CreateVertexBuffer(sizeof(data),      //Length
                                       D3DUSAGE_WRITEONLY,//Usage
                                       tri_fvf,           //FVF
                                       D3DPOOL_MANAGED,   //Pool
                                       &g_list_vb,        //ppVertexBuffer
                                       NULL);             //Handle
   if(FAILED(hr))
	{
      dhLog("Error Creating vertex buffer",hr);
      return hr;
   }

   hr=g_list_vb->Lock(0, //Offset
                      0, //SizeToLock
                      &vb_vertices, //Vertices
                      0);  //Flags
   if(FAILED(hr))
	{
      dhLog("Error Locking vertex buffer",hr);
      return hr;
   }

   memcpy( vb_vertices, data,sizeof(data));

   g_list_vb->Unlock();


   return D3D_OK;
}

//******************************************************************************************
// Function:default_window_proc
// Whazzit:This handles any incoming Windows messages and sends any that aren't handled to
//         DefWindowProc for Windows to handle.
//******************************************************************************************
LRESULT CALLBACK default_window_proc(HWND p_hwnd,UINT p_msg,WPARAM p_wparam,LPARAM p_lparam){
   
   switch(p_msg){
      case WM_KEYDOWN:
         if(p_wparam == VK_ESCAPE) //User hit Escape, end the app
			{ 
            g_app_done=true;
         }

		 if (p_wparam == VK_NUMPAD6)
		 {
			 x = x + 1.0;
		 }
		 if (p_wparam == VK_NUMPAD4)
		 {
			 x = x - 1.0;
		 }

         return 0;
      case WM_CLOSE:    //User hit the Close Window button, end the app
      case WM_LBUTTONDOWN: //user hit the left mouse button
         g_app_done=true;
         return 0;
   }
   
   return (DefWindowProc(p_hwnd,p_msg,p_wparam,p_lparam));
   
}

void move_cam(void) {
	//Here we build our View Matrix, think of it as our camera.

	//First we specify that our viewpoint is 8 units back on the Z-axis
	eye_vector = D3DXVECTOR3(0.0f, 0.0f, -8.0f);

	//We are looking towards the origin
	lookat_vector = D3DXVECTOR3(x, y, z);

	//The "up" direction is the positive direction on the y-axis
	up_vector = D3DXVECTOR3(0.0f, 1.0f, 0.0f);

	D3DXMatrixLookAtLH(&view_matrix, &eye_vector,
		&lookat_vector,
		&up_vector);

	//Since our 'camera' will never move, we can set this once at the
	//beginning and never worry about it again
	g_d3d_device->SetTransform(D3DTS_VIEW, &view_matrix);

	aspect = ((float)g_width / (float)g_height);

	D3DXMatrixPerspectiveFovLH(&projection_matrix, //Result Matrix
		D3DX_PI / 4,          //Field of View, in radians.
		aspect,             //Aspect ratio
		1.0f,               //Near view plane
		100.0f);           //Far view plane

						   //Our Projection matrix won't change either, so we set it now and never touch
						   //it again.
	g_d3d_device->SetTransform(D3DTS_PROJECTION, &projection_matrix);
}