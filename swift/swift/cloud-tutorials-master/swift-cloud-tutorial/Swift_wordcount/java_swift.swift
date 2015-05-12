type file;
#######################################################################################
#######################################################################################
file createCSV_jar<"createCSV.jar">;	# this jar would  create CSV file  for a given text file
file mergeCSV_jar<"mergeCSV.jar">;

(file t) createCSV_func(file m, file j) { 
	#trace(@j);
    app {
	java "-jar" @j @m stdout = @filename(t); 
	}
}
#######################################################################################
#######################################################################################

(file t) mergeCSV_func(file ip1,file ip2, file j) { 
	#trace(@j);
    app {
	java "-jar" @j @ip1 @ip2 stdout = @filename(t); 
	}
}

#######################################################################################
#######################################################################################



####----------------------------------------------------------------------------------

file inputfile[] <simple_mapper;prefix="x",padding=2>;	#array of input files
file outCSV_file[] <simple_mapper; prefix="a",suffix=".txt",padding=3>;	#output files with a formate a___.sem

trace(@length(inputfile));			 #to see the number of file in the input file
int filelen = @length(inputfile) - 1;
##-step-1------------
## for loop where  you are converting the text file to csv file with <,@> as delimitor 
foreach i in[0:filelen]{
	outCSV_file[i] = createCSV_func(inputfile[i], createCSV_jar); ## parameters are <inputu file, location of the jar file>
}


#file inputfile_merge[] <simple_mapper;prefix="a",suffix=".txt",padding=3>;	#array of input files
file outCSV_file_merge[] <simple_mapper; prefix="b",suffix=".txt",padding=3>;	#output files with a formate b___.txt

##
outCSV_file_merge[0] = createCSV_func(inputfile[0], createCSV_jar);

foreach e in[1:filelen]{
	outCSV_file_merge[e] = mergeCSV_func(outCSV_file_merge[e-1],outCSV_file[e], mergeCSV_jar);
	trace(e);
}

#file  finalOutput<"finalOutput.txt">;
#finalOutput = outCSV_file_merge[@length(outCSV_file_merge)-1];










