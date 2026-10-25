/* Probes vulkan-wsi-layer: present timing, surface formats, swapchains & presentation. */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef PROBE_WAYLAND
#include <wayland-client.h>
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#include <vulkan/vulkan.h>

static int failures;

#define CHECK(call)                                                                                        \
   do                                                                                                      \
   {                                                                                                       \
      VkResult r_ = (call);                                                                                \
      printf("  %-58s %s (%d)\n", #call, r_ == VK_SUCCESS ? "VK_SUCCESS" : "FAILED", r_);                  \
      if (r_ != VK_SUCCESS)                                                                                \
      {                                                                                                    \
         failures++;                                                                                       \
         goto out;                                                                                         \
      }                                                                                                    \
   } while (0)

static const char *wsi = "headless";
static uint32_t api = VK_API_VERSION_1_3;
static bool do_swapchain, do_timing, device_only, all_formats, dump_modifiers, modifier_image;
static int frames = 8;

static bool has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
   for (uint32_t i = 0; i < n; i++)
      if (strcmp(e[i].extensionName, name) == 0)
         return true;
   return false;
}

#ifdef PROBE_WAYLAND
static struct wl_compositor *compositor;
static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t v)
{
   (void)d;
   (void)v;
   if (strcmp(iface, "wl_compositor") == 0)
      compositor = wl_registry_bind(r, name, &wl_compositor_interface, 1);
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n)
{
   (void)d;
   (void)r;
   (void)n;
}
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };
#endif

static void usage(const char *argv0)
{
   fprintf(stderr,
           "usage: %s [-w headless|wayland|display] [-a 1.0|1.3] [-s] [-t] [-n frames]\n"
           "  -s  create a swapchain & present frames (not with wayland)\n"
           "  -t  request present timing (needs the swapchain flag; implies -s)\n"
           "  -d  skip the surface; only create the instance & device\n"
           "  -F  create (& destroy) a swapchain in every advertised surface format\n"
           "  -M  print the ICD's DRM format modifiers & their features for each surface format\n"
           "  -X  create a LINEAR-modifier image & read its modifier back (implies -d)\n",
           argv0);
   exit(2);
}

int main(int argc, char **argv)
{
   /* Line-buffered, so the output up to a loader abort() is kept. */
   setvbuf(stdout, NULL, _IOLBF, 0);
   int opt;
   while ((opt = getopt(argc, argv, "w:a:stdFMXn:")) != -1)
   {
      switch (opt)
      {
      case 'w': wsi = optarg; break;
      case 'a': api = strcmp(optarg, "1.0") == 0 ? VK_API_VERSION_1_0 : VK_API_VERSION_1_3; break;
      case 's': do_swapchain = true; break;
      case 't': do_timing = do_swapchain = true; break;
      case 'n': frames = atoi(optarg); break;
      case 'd': device_only = true; break;
      case 'F': all_formats = true; break;
      case 'M': dump_modifiers = true; break;
      case 'X': modifier_image = device_only = true; break;
      default: usage(argv[0]);
      }
   }
   bool headless = strcmp(wsi, "headless") == 0, wayland = strcmp(wsi, "wayland") == 0,
        display = strcmp(wsi, "display") == 0;
   if (!headless && !wayland && !display)
      usage(argv[0]);
#ifndef PROBE_WAYLAND
   if (wayland)
   {
      fprintf(stderr, "built without Wayland\n");
      return 2;
   }
#endif
   if (wayland && do_swapchain)
   {
      fprintf(stderr, "-s/-t not supported with wayland (no shell role); use vkcube\n");
      return 2;
   }

   printf("wsi=%s api=%s swapchain=%d timing=%d\n", wsi, api == VK_API_VERSION_1_0 ? "1.0" : "1.3", do_swapchain,
          do_timing);

   VkInstance inst = VK_NULL_HANDLE;
   VkDevice dev = VK_NULL_HANDLE;
   VkSurfaceKHR surf = VK_NULL_HANDLE;
   VkSwapchainKHR sc = VK_NULL_HANDLE;
   VkCommandPool pool = VK_NULL_HANDLE;
   VkSemaphore acq[16] = { 0 }, done[16] = { 0 };
   VkFence fence[16] = { 0 };
#ifdef PROBE_WAYLAND
   struct wl_display *wl = NULL;
   struct wl_surface *wls = NULL;
#endif

   const char *iexts[8];
   uint32_t ni = 0;
   iexts[ni++] = VK_KHR_SURFACE_EXTENSION_NAME;
   iexts[ni++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
   if (api == VK_API_VERSION_1_0)
      iexts[ni++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
   if (headless)
      iexts[ni++] = VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME;
#ifdef PROBE_WAYLAND
   if (wayland)
      iexts[ni++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif
   if (display)
      iexts[ni++] = VK_KHR_DISPLAY_EXTENSION_NAME;

   VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "wsi-probe", 1, NULL, 0, api };
   VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, ni, iexts };
   printf("[instance]\n");
   CHECK(vkCreateInstance(&ici, NULL, &inst));

   uint32_t npd = 0;
   CHECK(vkEnumeratePhysicalDevices(inst, &npd, NULL));
   VkPhysicalDevice pds[8];
   npd = npd > 8 ? 8 : npd;
   CHECK(vkEnumeratePhysicalDevices(inst, &npd, pds));
   int gpu = getenv("PROBE_GPU") ? atoi(getenv("PROBE_GPU")) : 0;
   if (gpu < 0 || (uint32_t)gpu >= npd)
   {
      printf("  no physical device %d (%u found)\n", gpu, npd);
      failures++;
      goto out;
   }
   VkPhysicalDevice pd = pds[gpu];
   VkPhysicalDeviceProperties props;
   vkGetPhysicalDeviceProperties(pd, &props);
   printf("[device] %s, driver API %u.%u\n", props.deviceName, VK_VERSION_MAJOR(props.apiVersion),
          VK_VERSION_MINOR(props.apiVersion));

   uint32_t ne = 0;
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
   VkExtensionProperties *de = calloc(ne, sizeof(*de));
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, de);
   const char *watch[] = { VK_KHR_MAINTENANCE_9_EXTENSION_NAME,           VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
                           VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,    VK_EXT_PRESENT_TIMING_EXTENSION_NAME,
                           VK_KHR_PRESENT_ID_2_EXTENSION_NAME,             VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
                           VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,  VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
                           VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,       VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
                           VK_KHR_SWAPCHAIN_EXTENSION_NAME,                VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME };
   printf("[device extensions as seen by the app] %u total\n", ne);
   for (size_t i = 0; i < sizeof(watch) / sizeof(watch[0]); i++)
      printf("  %-40s %s\n", watch[i], has_ext(de, ne, watch[i]) ? "yes" : "no");

   PFN_vkGetPhysicalDeviceFeatures2KHR gpf2 =
      (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(
         inst, api == VK_API_VERSION_1_0 ? "vkGetPhysicalDeviceFeatures2KHR" : "vkGetPhysicalDeviceFeatures2");
   VkPhysicalDevicePresentTimingFeaturesEXT ptf = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT };
   VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &ptf };
   gpf2(pd, &f2);
   printf("[present timing features] presentTiming=%u presentAtAbsoluteTime=%u presentAtRelativeTime=%u\n",
          ptf.presentTiming, ptf.presentAtAbsoluteTime, ptf.presentAtRelativeTime);

   if (device_only)
      goto create_device;
   printf("[surface]\n");
   VkExtent2D extent = { 256, 256 };
   if (headless)
   {
      PFN_vkCreateHeadlessSurfaceEXT chs =
         (PFN_vkCreateHeadlessSurfaceEXT)vkGetInstanceProcAddr(inst, "vkCreateHeadlessSurfaceEXT");
      VkHeadlessSurfaceCreateInfoEXT hci = { VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT };
      CHECK(chs(inst, &hci, NULL, &surf));
   }
#ifdef PROBE_WAYLAND
   if (wayland)
   {
      wl = wl_display_connect(NULL);
      if (!wl)
      {
         printf("  wl_display_connect failed (is WAYLAND_DISPLAY set?)\n");
         failures++;
         goto out;
      }
      struct wl_registry *reg = wl_display_get_registry(wl);
      wl_registry_add_listener(reg, &reg_listener, NULL);
      wl_display_roundtrip(wl);
      if (!compositor)
      {
         printf("  the compositor does not advertise wl_compositor\n");
         failures++;
         goto out;
      }
      wls = wl_compositor_create_surface(compositor);
      VkWaylandSurfaceCreateInfoKHR wci = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, NULL, 0, wl, wls };
      CHECK(vkCreateWaylandSurfaceKHR(inst, &wci, NULL, &surf));
   }
#endif
   if (display)
   {
      uint32_t nd = 0;
      CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &nd, NULL));
      if (nd == 0)
      {
         printf("  no displays (HDMI unplugged, or another process is DRM master?)\n");
         failures++;
         goto out;
      }
      VkDisplayPropertiesKHR dp[4];
      nd = nd > 4 ? 4 : nd;
      CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &nd, dp));
      uint32_t nm = 0;
      CHECK(vkGetDisplayModePropertiesKHR(pd, dp[0].display, &nm, NULL));
      VkDisplayModePropertiesKHR mp[64];
      nm = nm > 64 ? 64 : nm;
      CHECK(vkGetDisplayModePropertiesKHR(pd, dp[0].display, &nm, mp));
      printf("  display '%s', mode %ux%u@%u\n", dp[0].displayName ? dp[0].displayName : "?",
             mp[0].parameters.visibleRegion.width, mp[0].parameters.visibleRegion.height,
             mp[0].parameters.refreshRate);
      extent = mp[0].parameters.visibleRegion;
      VkDisplaySurfaceCreateInfoKHR dci = { VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR };
      dci.displayMode = mp[0].displayMode;
      dci.planeIndex = 0;
      dci.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
      dci.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
      dci.globalAlpha = 1.0f;
      dci.imageExtent = extent;
      CHECK(vkCreateDisplayPlaneSurfaceKHR(inst, &dci, NULL, &surf));
   }

   PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR gsc2 = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
      vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
   VkPhysicalDeviceSurfaceInfo2KHR si = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, NULL, surf };

   /* Query each chained struct on its own so a failure is attributed to the right one. */
   VkSwapchainFlagsSurfaceCapabilitiesEXT scf = { VK_STRUCTURE_TYPE_SWAPCHAIN_FLAGS_SURFACE_CAPABILITIES_EXT };
   VkSurfaceCapabilities2KHR c2 = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, &scf };
   VkResult r = gsc2(pd, &si, &c2);
   printf("[surface caps + VkSwapchainFlagsSurfaceCapabilitiesEXT] %d, swapchainSupportedFlags=0x%x, "
          "PRESENT_TIMING_BIT=%s\n",
          r, scf.swapchainSupportedFlags,
          (scf.swapchainSupportedFlags & VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT) ? "yes" : "no");
   bool timing_flag = r == VK_SUCCESS && (scf.swapchainSupportedFlags & VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT);
   VkSurfaceCapabilitiesKHR caps = c2.surfaceCapabilities;

   VkPresentTimingSurfaceCapabilitiesEXT ptc = { VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT };
   ptc.presentTimingSupported = 0xdead;
   ptc.presentAtAbsoluteTimeSupported = 0xdead;
   ptc.presentAtRelativeTimeSupported = 0xdead;
   ptc.presentStageQueries = 0xdead;
   VkSurfaceCapabilities2KHR c3 = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, &ptc };
   r = gsc2(pd, &si, &c3);
   printf("[surface caps + VkPresentTimingSurfaceCapabilitiesEXT] %d, presentTimingSupported=0x%x "
          "absolute=0x%x relative=0x%x presentStageQueries=0x%x\n",
          r, ptc.presentTimingSupported, ptc.presentAtAbsoluteTimeSupported, ptc.presentAtRelativeTimeSupported,
          ptc.presentStageQueries);
   if (r != VK_SUCCESS)
      failures++;

   uint32_t nf = 0;
   CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nf, NULL));
   VkSurfaceFormatKHR fmts[128];
   nf = nf > 128 ? 128 : nf;
   CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nf, fmts));
   printf("[surface formats] %u:", nf);
   for (uint32_t i = 0; i < nf; i++)
      printf(" %d/%d", fmts[i].format, fmts[i].colorSpace);
   printf("\n");

   if (dump_modifiers)
   {
      PFN_vkGetPhysicalDeviceFormatProperties2KHR gfp2 = (PFN_vkGetPhysicalDeviceFormatProperties2KHR)
         vkGetInstanceProcAddr(inst, api == VK_API_VERSION_1_0 ? "vkGetPhysicalDeviceFormatProperties2KHR"
                                                               : "vkGetPhysicalDeviceFormatProperties2");
      for (uint32_t i = 0; i < nf && gfp2; i++)
      {
         VkDrmFormatModifierPropertiesEXT mods[32];
         VkDrmFormatModifierPropertiesListEXT ml = { VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT };
         VkFormatProperties2 fp = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &ml };
         gfp2(pd, fmts[i].format, &fp);
         uint32_t nm = ml.drmFormatModifierCount > 32 ? 32 : ml.drmFormatModifierCount;
         ml.drmFormatModifierCount = nm;
         ml.pDrmFormatModifierProperties = mods;
         gfp2(pd, fmts[i].format, &fp);
         printf("[format %d] optimal=0x%x linear=0x%x, %u modifiers:", fmts[i].format,
                fp.formatProperties.optimalTilingFeatures, fp.formatProperties.linearTilingFeatures, nm);
         for (uint32_t m = 0; m < nm; m++)
            printf(" 0x%llx(planes=%u features=0x%x)", (unsigned long long)mods[m].drmFormatModifier,
                   mods[m].drmFormatModifierPlaneCount, mods[m].drmFormatModifierTilingFeatures);
         printf("\n");

         VkDrmFormatModifierProperties2EXT mods2[32];
         VkDrmFormatModifierPropertiesList2EXT ml2 = { VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT };
         VkFormatProperties2 fp2 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &ml2 };
         gfp2(pd, fmts[i].format, &fp2);
         uint32_t nm2 = ml2.drmFormatModifierCount > 32 ? 32 : ml2.drmFormatModifierCount;
         ml2.drmFormatModifierCount = nm2;
         ml2.pDrmFormatModifierProperties = mods2;
         gfp2(pd, fmts[i].format, &fp2);
         printf("[format %d] List2: %u modifiers:", fmts[i].format, nm2);
         for (uint32_t m = 0; m < nm2; m++)
            printf(" 0x%llx(features=0x%llx)", (unsigned long long)mods2[m].drmFormatModifier,
                   (unsigned long long)mods2[m].drmFormatModifierTilingFeatures);
         printf("\n");
      }
   }

   VkBool32 present_ok = VK_FALSE;
   vkGetPhysicalDeviceSurfaceSupportKHR(pd, 0, surf, &present_ok);
   printf("[queue family 0 can present] %u\n", present_ok);

create_device:;
   const char *dexts[8];
   uint32_t nde = 0;
   dexts[nde++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
   VkPhysicalDevicePresentTimingFeaturesEXT ptf_en = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT };
   void *dnext = NULL;
   if (do_timing)
   {
      /* VK_EXT_present_timing depends on VK_KHR_present_id2 & VK_KHR_calibrated_timestamps. */
      if (!ptf.presentTiming || !has_ext(de, ne, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) ||
          !has_ext(de, ne, VK_KHR_PRESENT_ID_2_EXTENSION_NAME))
      {
         printf("[present timing] not usable on this device; a valid app cannot request it, skipping -t\n");
         do_timing = false;
      }
      else
      {
         dexts[nde++] = VK_EXT_PRESENT_TIMING_EXTENSION_NAME;
         dexts[nde++] = VK_KHR_PRESENT_ID_2_EXTENSION_NAME;
         dexts[nde++] = VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME;
         ptf_en.presentTiming = VK_TRUE;
         dnext = &ptf_en;
      }
   }
   if (modifier_image)
   {
      dexts[nde++] = VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME;
      dexts[nde++] = VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME;
   }
   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio };
   VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, dnext, 0, 1, &qci, 0, NULL, nde, dexts };
   printf("[device creation]\n");
   CHECK(vkCreateDevice(pd, &dci, NULL, &dev));

   if (modifier_image)
   {
      uint64_t linear = 0;
      VkImageDrmFormatModifierListCreateInfoEXT mlci = { VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
                                                         NULL, 1, &linear };
      VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &mlci };
      ii.imageType = VK_IMAGE_TYPE_2D;
      ii.format = VK_FORMAT_R5G6B5_UNORM_PACK16;
      ii.extent = (VkExtent3D){ 64, 64, 1 };
      ii.mipLevels = ii.arrayLayers = 1;
      ii.samples = VK_SAMPLE_COUNT_1_BIT;
      ii.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
      ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      VkImage img = VK_NULL_HANDLE;
      printf("[LINEAR-modifier image]\n");
      CHECK(vkCreateImage(dev, &ii, NULL, &img));
      PFN_vkGetImageDrmFormatModifierPropertiesEXT gimp = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)
         vkGetDeviceProcAddr(dev, "vkGetImageDrmFormatModifierPropertiesEXT");
      VkImageDrmFormatModifierPropertiesEXT imp = { VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT };
      imp.drmFormatModifier = 0xdeadbeefull;
      VkResult ir = gimp ? gimp(dev, img, &imp) : VK_ERROR_EXTENSION_NOT_PRESENT;
      printf("  vkGetImageDrmFormatModifierPropertiesEXT %p -> %d, modifier=0x%llx\n", (void *)gimp, ir,
             (unsigned long long)imp.drmFormatModifier);
      if (ir != VK_SUCCESS || imp.drmFormatModifier != 0)
         failures++;
      vkDestroyImage(dev, img, NULL);
   }

   if (all_formats && !device_only)
   {
      VkExtent2D fext = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : extent;
      uint32_t bad = 0;
      printf("[swapchain per advertised format] %u formats, %ux%u\n", nf, fext.width, fext.height);
      for (uint32_t i = 0; i < nf; i++)
      {
         VkSwapchainCreateInfoKHR fci_ = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
         fci_.surface = surf;
         fci_.minImageCount = caps.minImageCount < 2 ? 2 : caps.minImageCount;
         fci_.imageFormat = fmts[i].format;
         fci_.imageColorSpace = fmts[i].colorSpace;
         fci_.imageExtent = fext;
         fci_.imageArrayLayers = 1;
         fci_.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
         fci_.preTransform = caps.currentTransform;
         fci_.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
         fci_.presentMode = VK_PRESENT_MODE_FIFO_KHR;
         fci_.clipped = VK_TRUE;
         VkSwapchainKHR fsc = VK_NULL_HANDLE;
         VkResult fr = vkCreateSwapchainKHR(dev, &fci_, NULL, &fsc);
         uint32_t fn = 0;
         if (fr == VK_SUCCESS)
            fr = vkGetSwapchainImagesKHR(dev, fsc, &fn, NULL);
         printf("  format %d/%d: %s (%d), %u images\n", fmts[i].format, fmts[i].colorSpace,
                fr == VK_SUCCESS ? "ok" : "FAILED", fr, fn);
         if (fr != VK_SUCCESS)
            bad++;
         if (fsc)
            vkDestroySwapchainKHR(dev, fsc, NULL);
      }
      printf("[swapchain per advertised format] %u of %u failed\n", bad, nf);
      failures += (int)bad;
   }

   if (!do_swapchain || device_only)
      goto out;

   printf("[swapchain]\n");
   if (caps.currentExtent.width != UINT32_MAX)
      extent = caps.currentExtent;
   VkSwapchainCreateInfoKHR sci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
   sci.surface = surf;
   sci.minImageCount = caps.minImageCount < 2 ? 2 : caps.minImageCount;
   sci.imageFormat = fmts[0].format;
   sci.imageColorSpace = fmts[0].colorSpace;
   sci.imageExtent = extent;
   sci.imageArrayLayers = 1;
   sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   sci.preTransform = caps.currentTransform;
   sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
   sci.clipped = VK_TRUE;
   if (do_timing)
   {
      if (!timing_flag)
         printf("  PRESENT_TIMING_BIT not advertised; requesting it anyway\n");
      sci.flags |= VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT;
   }
   printf("  format %d, %ux%u, %u images, flags 0x%x\n", sci.imageFormat, extent.width, extent.height,
          sci.minImageCount, sci.flags);
   CHECK(vkCreateSwapchainKHR(dev, &sci, NULL, &sc));

   PFN_vkSetSwapchainPresentTimingQueueSizeEXT set_qs = NULL;
   PFN_vkGetPastPresentationTimingEXT get_past = NULL;
   PFN_vkGetSwapchainTimingPropertiesEXT get_props = NULL;
   PFN_vkGetCalibratedTimestampsKHR get_cal = NULL;
   if (do_timing)
   {
      set_qs = (PFN_vkSetSwapchainPresentTimingQueueSizeEXT)vkGetDeviceProcAddr(dev,
                                                                                "vkSetSwapchainPresentTimingQueueSizeEXT");
      get_past = (PFN_vkGetPastPresentationTimingEXT)vkGetDeviceProcAddr(dev, "vkGetPastPresentationTimingEXT");
      get_props =
         (PFN_vkGetSwapchainTimingPropertiesEXT)vkGetDeviceProcAddr(dev, "vkGetSwapchainTimingPropertiesEXT");
      get_cal = (PFN_vkGetCalibratedTimestampsKHR)vkGetDeviceProcAddr(dev, "vkGetCalibratedTimestampsKHR");
      printf("  entry points: queue_size=%p past=%p props=%p calibrated=%p\n", (void *)set_qs, (void *)get_past,
             (void *)get_props, (void *)get_cal);
      if (!set_qs || !get_past || !get_props || !get_cal)
      {
         failures++;
         goto out;
      }
      CHECK(set_qs(dev, sc, (uint32_t)frames));
   }

   uint32_t nimg = 0;
   CHECK(vkGetSwapchainImagesKHR(dev, sc, &nimg, NULL));
   VkImage imgs[16];
   nimg = nimg > 16 ? 16 : nimg;
   CHECK(vkGetSwapchainImagesKHR(dev, sc, &nimg, imgs));
   VkQueue q;
   vkGetDeviceQueue(dev, 0, 0, &q);
   VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0 };
   CHECK(vkCreateCommandPool(dev, &pci, NULL, &pool));
   VkCommandBuffer cbs[16];
   VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool,
                                       VK_COMMAND_BUFFER_LEVEL_PRIMARY, nimg };
   CHECK(vkAllocateCommandBuffers(dev, &cai, cbs));
   VkSemaphoreCreateInfo semci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
   VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT };
   for (uint32_t i = 0; i < nimg; i++)
   {
      CHECK(vkCreateSemaphore(dev, &semci, NULL, &acq[i]));
      CHECK(vkCreateSemaphore(dev, &semci, NULL, &done[i]));
      CHECK(vkCreateFence(dev, &fci, NULL, &fence[i]));
   }

   int presented = 0;
   for (int f = 0; f < frames; f++)
   {
      uint32_t slot = (uint32_t)f % nimg, idx;
      vkWaitForFences(dev, 1, &fence[slot], VK_TRUE, UINT64_MAX);
      VkResult ar = vkAcquireNextImageKHR(dev, sc, 5000000000ull, acq[slot], VK_NULL_HANDLE, &idx);
      if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
      {
         printf("  frame %d: vkAcquireNextImageKHR %d\n", f, ar);
         failures++;
         break;
      }
      if (idx >= nimg)
      {
         printf("  frame %d: image index %u, but only %u images were queried\n", f, idx, nimg);
         failures++;
         break;
      }
      vkResetFences(dev, 1, &fence[slot]);
      VkCommandBuffer cb = cbs[slot];
      vkResetCommandBuffer(cb, 0);
      VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      vkBeginCommandBuffer(cb, &bi);
      VkImageSubresourceRange rng = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
      VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
      b.srcAccessMask = 0;
      b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      b.image = imgs[idx];
      b.subresourceRange = rng;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                           1, &b);
      VkClearColorValue col = { .float32 = { (float)(f & 1), 0.5f, (float)((f >> 1) & 1), 1.0f } };
      vkCmdClearColorImage(cb, imgs[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &col, 1, &rng);
      b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      b.dstAccessMask = 0;
      b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                           NULL, 1, &b);
      vkEndCommandBuffer(cb);
      VkPipelineStageFlags ws = VK_PIPELINE_STAGE_TRANSFER_BIT;
      VkSubmitInfo sub = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 1, &acq[slot], &ws, 1, &cb, 1, &done[idx] };
      vkQueueSubmit(q, 1, &sub, fence[slot]);

      VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, &done[idx], 1, &sc, &idx, NULL };
      VkPresentTimingInfoEXT pti = { VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT };
      VkPresentTimingsInfoEXT ptis = { VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT, NULL, 1, &pti };
      if (do_timing)
      {
         pti.presentStageQueries = ptc.presentStageQueries;
         pi.pNext = &ptis;
      }
      VkResult pr = vkQueuePresentKHR(q, &pi);
      if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR)
      {
         printf("  frame %d: vkQueuePresentKHR %d\n", f, pr);
         failures++;
         break;
      }
      presented++;
   }
   vkDeviceWaitIdle(dev);
   printf("  presented %d/%d frames\n", presented, frames);

   if (do_timing)
   {
      VkSwapchainTimingPropertiesEXT stp = { VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT };
      uint64_t counter = 0;
      r = get_props(dev, sc, &stp, &counter);
      printf("  vkGetSwapchainTimingPropertiesEXT %d, refreshDuration=%llu refreshInterval=%llu\n", r,
             (unsigned long long)stp.refreshDuration, (unsigned long long)stp.refreshInterval);
      if (r != VK_SUCCESS && r != VK_NOT_READY)
         failures++;

      VkPastPresentationTimingEXT past[64];
      VkPresentStageTimeEXT stages[64][4];
      memset(past, 0, sizeof(past));
      for (int i = 0; i < 64; i++)
      {
         past[i].sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT;
         past[i].presentStageCount = 4;
         past[i].pPresentStages = stages[i];
      }
      VkPastPresentationTimingInfoEXT ppi = { VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT, NULL, 0, sc };
      VkPastPresentationTimingPropertiesEXT ppp = { VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT };
      ppp.presentationTimingCount = 64;
      ppp.pPresentationTimings = past;
      r = get_past(dev, &ppi, &ppp);
      printf("  vkGetPastPresentationTimingEXT %d, %u results\n", r, ppp.presentationTimingCount);
      if (r != VK_SUCCESS && r != VK_INCOMPLETE)
         failures++;
      for (uint32_t i = 0; i < ppp.presentationTimingCount && i < 3; i++)
         printf("    #%u complete=%u stages=%u first=0x%x@%llu\n", i, past[i].reportComplete,
                past[i].presentStageCount, past[i].presentStageCount ? stages[i][0].stage : 0,
                past[i].presentStageCount ? (unsigned long long)stages[i][0].time : 0ull);

      VkCalibratedTimestampInfoKHR cti = { VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, NULL,
                                           VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR };
      uint64_t ts = 0, dev_ = 0;
      r = get_cal(dev, 1, &cti, &ts, &dev_);
      printf("  vkGetCalibratedTimestampsKHR(CLOCK_MONOTONIC) %d, ts=%llu\n", r, (unsigned long long)ts);
      if (r != VK_SUCCESS)
         failures++;
   }

out:
   if (dev)
   {
      vkDeviceWaitIdle(dev);
      for (int i = 0; i < 16; i++)
      {
         if (acq[i])
            vkDestroySemaphore(dev, acq[i], NULL);
         if (done[i])
            vkDestroySemaphore(dev, done[i], NULL);
         if (fence[i])
            vkDestroyFence(dev, fence[i], NULL);
      }
      if (pool)
         vkDestroyCommandPool(dev, pool, NULL);
      if (sc)
         vkDestroySwapchainKHR(dev, sc, NULL);
      vkDestroyDevice(dev, NULL);
   }
   if (surf)
      vkDestroySurfaceKHR(inst, surf, NULL);
   if (inst)
      vkDestroyInstance(inst, NULL);
#ifdef PROBE_WAYLAND
   if (wls)
      wl_surface_destroy(wls);
   if (wl)
      wl_display_disconnect(wl);
#endif
   printf("RESULT: %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
