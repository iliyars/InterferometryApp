import cv2, numpy as np
img = cv2.imread(r'D:\dev\cpp\InterferometryApp\test_images\22.bmp', 0)
_, binary = cv2.threshold(img, 0, 255, cv2.THRESH_OTSU)
skeleton = cv2.ximgproc.thinning(binary)
cv2.imwrite(r'D:\dev\cpp\InterferometryApp\test_images\22_scelet.bmp', skeleton)
