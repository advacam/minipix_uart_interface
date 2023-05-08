import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import math
import os
from os import path
import re
import sys
from matplotlib.colors import LogNorm


def load_data(file_in_path_name, val_1_max = 1022):

		print("* Loading file:\t\t", file_in_path_name)

		data = []

		try:
			with open(file_in_path_name, 'r') as file_in:
				n_line = 0
				frame_num = 0

				frame = np.zeros((256,256))

				for line in file_in:

					n_line += 1

					line = line.replace("\n", "")					
					line_list = line.split("\t")

					if line[0] == 'f':
						line_list = line.split(" ")
						if len(line_list) >= 2:
							frame_num = int(line_list[1])
						if np.sum(frame) != 0:
							data.append(frame)
						frame = np.zeros((256,256))
						continue

					x = -1
					y = -1
					val_1 = -1
					val_2 = -1

					if len(line_list) >= 4:
						x = int(line_list[0])
						y = int(line_list[1])
						val_1 = float(line_list[2])
						val_2 = float(line_list[3])

						if (val_1 == 1022):
							print("[WARINING] Pixel with high response (x,y,tot, line, frame):" , x, y, val_1, n_line, frame_num)

						if (x == 26 and y == 120) or (x == 26 and y == 0):
							continue

						frame[x,y] = val_1

				if np.sum(frame) != 0:
					data.append(frame)

		except IOError:
			print("Can not open file: " + file_in_path_name )	
			return -1, data

		return 0, data


def matrix_center(matrix):

	sum_x = 0
	sum_y = 0
	sum_val = 0

	i = 0
	j = 0

	for arr in matrix:
		for num in arr:
			j += 1
			sum_x += num*float(i+0.5)
			sum_y += num*float(j+0.5)

			sum_val += num

		i += 1
		j = 0

	center_x = -1 
	center_y = -1

	if sum_val != 0:
		center_x = sum_x/float(sum_val)
		center_y = sum_y/float(sum_val)

	return center_x, center_y

def analyse_frame(matrix):

	shape = matrix.shape

	matrix_sum = np.sum(matrix)
	n_px = np.count_nonzero(matrix)
	occupancy = 100.*float(n_px)/(shape[0]*shape[1])
	max_val = np.max(matrix)	 	

	return matrix_sum, n_px, occupancy, max_val


def analyse_frames(frames, file_out_path):

	print("* Basic analysis")

	list_sum = []
	list_n_pix = []
	list_occ = []
	list_max = []

	list_fr_num = [i for i in range(len(frames))]

	sum_min = 80000
	sum_max = 110000
	max_min = 100
	max_max = 200	

	for frame in frames:

		fr_sum, fr_n_pix, fr_occ, fr_max = analyse_frame(frame)

		# print(fr_sum, fr_n_pix, fr_occ, fr_max)

		list_sum.append(fr_sum)
		list_n_pix.append(fr_n_pix)
		list_occ.append(fr_occ)
		list_max.append(fr_max)

		if fr_sum < sum_min:
			print("[WARINING] Frame has low sum (sum, frame):" , fr_sum, frame_num)
		if fr_sum > sum_max:
			print("[WARINING] Frame has high sum (sum, frame):" , fr_sum, frame_num)
		if fr_max < max_min:
			print("[WARINING] Frame has low max (sum, frame):" , fr_max, frame_num)
		if fr_max > max_max:
			print("[WARINING] Frame has high max (sum, frame):" , fr_max, frame_num)
							

	plot_graph_1d(file_out_path + "sum.png", list_fr_num, list_sum, "sum", "frame", "sum", sum_min  , sum_max )
	plot_graph_1d(file_out_path + "max.png", list_fr_num, list_max, "max", "frame", "max", max_min , max_max )
	plot_graph_1d(file_out_path + "occupancy.png", list_fr_num, list_occ, "occupancy", "frame", "occupancy", 5 , 7.5 )


#Plot 1D graph with linear fuction defined with A_Slope and B_Shift
def plot_graph_1d(FileOut_PathName, ListX, ListY, Title, LabelX, LabelY, min_val = -1, max_val = -1):

	#Check whether the size of the Listtor are same -> if not then set the size as the smaller one
	# if len(ListX) != len(ListY) :
	# 	if len(ListX) > len(ListY):
	# 		ListX = ListX[:len(ListY)]
	# 	else:
	# 		ListY = ListY[:len(ListX)]

	#Main plot function
	fig, ax = plt.subplots()
	ax.plot(ListX,ListY, color='gainsboro', linewidth=0,
			marker='o', markerfacecolor='dodgerblue', markeredgewidth=0,
			markersize=5)

	#Plot fit with linear funciton
	# (A_Slope,B_Shift), FitParStdErr = LinFuncFit(ListX, ListY) #Fit with linear fuction
	# LegengLinearFunc = LegedLabelLinFuncFit(A_Slope,B_Shift,FitParStdErr)
	# plt.plot(ListX,LinFucnList(ListX,A_Slope, B_Shift), 'r',linestyle='--', label=LegendData+" "+LegengLinearFunc,linewidth=0.5)

	#Additional settings 
	plt.xlabel(LabelX)
	plt.ylabel(LabelY)
	plt.title(Title)
	#plt.text(10, 144, r'an equation: $E=mc^2$', fontsize=15)
	# (Ymin,Ymax) = GetMinMaxListVal(ListY)
	# plt.ylim(ymin=Ymin-(Ymax-Ymin)*0.2, ymax = Ymax+(Ymax-Ymin)*0.4)
	#plt.legend(bbox_to_anchor=(0.01,0.9), loc="center left", borderaxespad=1, frameon=False)
	# plt.legend(fontsize=10)
	plt.grid(visible = True, color ='grey',  linestyle ='-.', linewidth = 0.5,  alpha = 0.6)

	if max_val != -1:
		ax.axhline(y=max_val, color='red', linestyle='--')

	if min_val != -1:
		ax.axhline(y=min_val, color='red', linestyle='--')

	#fig = plt.gcf()
	#fig.set_size_inches(14.5, 8.5)

	#Save and close file
	plt.savefig(FileOut_PathName, dpi=600)
	plt.close()
	# plt.show()


def plot_matrix(matrix, file_out_path_name, do_log_z = False, names = [], values = []):

	matrix_sum = np.sum(matrix)
	names.append("sum")
	values.append(matrix_sum)

	if matrix_sum == 0:
		return

	n_px = np.count_nonzero(matrix)
	names.append("pixel count")
	values.append(n_px)	 

	shape = matrix.shape
	occupancy = 100.*float(n_px)/(shape[0]*shape[1])
	names.append("occupancy")
	values.append(occupancy)	

	max_val = np.max(matrix)
	names.append("maximum value")
	values.append(max_val)		 	

	
	# fig, ax = plt.subplots()
	fig = plt.figure(num=None, figsize=(6, 6), dpi=300, facecolor='w', edgecolor='k')
	ax = fig.add_subplot(111)
	if do_log_z:
		plt.imshow(matrix, cmap = 'viridis', aspect = 'auto', origin='lower', norm=LogNorm())
	else:
		plt.imshow(matrix, cmap = 'viridis', aspect = 'auto', origin='lower')		
	plt.colorbar()
	plt.axis('square')

	if len(names) != 0:
		props = dict(boxstyle='round', facecolor='white', alpha=0.7, linewidth=0 )
		plt.subplots_adjust(right=0.65)
		fig = plt.gcf()
		fig.set_size_inches(7.5, 4.4)

		x_par_step = 1./30.;
		if len(names) > 30:
			x_par_step = 1./float(len(names))
		i = 0

		name_width = max(len(name) for name in names)
		value_width = 1  # adjust as needed
		# text_str = '\n'.join([f'{key:<{name_width}} = {value:>{value_width}.2f}' for key, value in zip(names, values)])
		text_str = ""
		text_str = '\n'.join([f'{key:<{name_width}} = {value:>{value_width}}' for key, value in zip(names, values)])

		plt.text(1.4, 0.5 , text_str, multialignment='left', transform=ax.transAxes, 
				alpha=0.7, bbox=props, fontsize=7, family='DejaVu Sans Mono')



	# plt.show()
	plt.savefig(file_out_path_name)
	plt.close()


if __name__ == '__main__':

	#  input variables
	file_in_path_name = "./example_interface/linux_test/out/test/data.txt"
	file_out_path  = "./example_interface/linux_test/out/test/"

	# information print
	print("\n=================================================================")
	print("                           DATA ANALYSIS                           ")
	print("=================================================================\n\n")
	print("Processed file:\t\t" ,file_in_path_name)
	print("Outpu to directory:\t", file_out_path)
	print("\n")


	#  load data
	rc, data = load_data(file_in_path_name, 500)

	# basic analysis

	analyse_frames(data, file_out_path)

	# plot data 
	matrix_sum = np.zeros((256,256))

	i = 0
	for frame in data:
		file_out_path_name = file_out_path + "frame_" + str(i) + ".png"
		plot_matrix(frame, file_out_path_name, False, ["frame"], [i])
		i += 1
		matrix_sum += frame

	plot_matrix(matrix_sum, file_out_path + "frame_sum_log.png")
	plot_matrix(matrix_sum, file_out_path + "frame_sum.png", False)

