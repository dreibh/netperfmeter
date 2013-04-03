# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

args<-commandArgs(TRUE)

folders			<-	list ("DFN-Cernet-SP", "DFN-Unicom-SP", "Versatel-Cernet-SP", "Versatel-Unicom-SP", "DFN-Cernet-CMT", "DFN-Unicom-CMT", "Versatel-Cernet-CMT", "Versatel-unicom-CMT", "DFN-Cernet-mptcp", "DFN-Unicom-mptcp", "Versatel-Cernet-mptcp", "Versatel-Unicom-mptcp")
protocols		<-	list("Single-Path", "Single-Path", "Single-Path", "Single-Path", "CMT-SCTP", "CMT-SCTP", "CMT-SCTP", "CMT-SCTP", "MPTCP", "MPTCP", "MPTCP", "MPTCP")
paths			<-	list("DFN-Cernet", "DFN-Unicom", "Versatel-Cernet", "Versatel-Unicom", "DFN-Cernet", "DFN-Unicom", "Versatel-Cernet", "Versatel-unicom", "DFN-Cernet", "DFN-Unicom", "Versatel-Cernet", "Versatel-Unicom")	
outputFile		<-	"/media/storage/Dropbox/Ubuntu-VM-Prince/essen-haikou/Results/passive.flow-ReceivedByteRate.data"
simulationDirectory	<-	"/media/storage/svn-git/projects/sctp/sctp-misc/trunk/netperfmeter/src/hakim-tests/haikou-paper/essen-haikou"


tmpData <- NULL
folderCounter<-0

ls

for(oneFolder in list ("DFN-Cernet-SP", "DFN-Unicom-SP", "Versatel-Cernet-SP", "Versatel-Unicom-SP")) {
	
	resultFileNamebz2  	<- 	paste(sep="", simulationDirectory, "/", oneFolder, "/Results/passive.flow-ReceivedByteRate.data.bz2")
	resultFileName  	<- 	paste(sep="", simulationDirectory, "/", oneFolder, "/Results/passive.flow-ReceivedByteRate.data")
	print(resultFileName)
	
	system("tar jxf resultFileName
	

	
	
	
	
	a	<-	read.table(resultFileName, header=T, quote="\"")
#  	a	<-	NULL
#  	a$passive.flow.ReceivedByteRate		<-	b$passive.flow.ReceivedByteRate
#  	
#  	print(length(a$passive.flow.ReceivedByteRate))
# 	len<-length(a$passive.flow.ReceivedByteRate)
#   	a <- transform(a, Protocol=protocols[folderCounter])
# 	colnames(a)[len+24+1]<-"Protocol"
#  	a <- transform(a, Path=paths[folderCounter])
#  	colnames(a)[len+24+3]<-"Path"
#  	print(a)

	tmpData <- append(tmpData, list(a))

}


sink(outputFile)
tmpData
sink(NULL) 

system("rm /media/storage/Dropbox/Ubuntu-VM-Prince/essen-haikou/Results/*.bz2 & bzip2 -f /media/storage/Dropbox/Ubuntu-VM-Prince/essen-haikou/Results/passive.flow-ReceivedByteRate.data", intern=TRUE)


