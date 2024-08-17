#pragma once
#include "Types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<stdint.h>
static Lakes lakes = { 0 };
static Mounts mounts = { 0 };

static Militaries militaries = { 0 };
static Aerodromes aerodromes = { 0 };

void dataManager(Metadata* metadata);
//void loadLakes(Metadata* metadata);
//void loadMounts();
void loadRivers(metadata);
//void loadMilitaries();
//void loadAerodromes();
//void sortRespectToWayId();

void loadRivers(Metadata* metadata) {
	Rivers rivers = { 0 };
	//Count number of rivers
	int numberOfRivers = 0;
	int numberOfMembers = 0;
	int index = 0;
	int coordinateIndex = 0;
	for (int i = 0; i < metadata->relations.count; i++) {
		for (int j = 0; j < metadata->relations.relations[i].tagCount; j++) {
			if (((strcmp((metadata->relations.relations[i].tags[j].k), "water") == 0)|| (strcmp((metadata->relations.relations[i].tags[j].k), "waterway") == 0))
				&& (strcmp((metadata->relations.relations[i].tags[j].v), "river") == 0)) {
				numberOfRivers++;
			}
		}
	}
	rivers.count = numberOfRivers;
	rivers.riverList = (River*)malloc(numberOfRivers * sizeof(River));

	for (int i = 0; i < metadata->relations.count; i++) {
		for (int j = 0; j < metadata->relations.relations[i].tagCount; j++) {
			if (((strcmp((metadata->relations.relations[i].tags[j].k), "water") == 0) || (strcmp((metadata->relations.relations[i].tags[j].k), "waterway") == 0))
				&& (strcmp((metadata->relations.relations[i].tags[j].v), "river") == 0)) {
				numberOfMembers = metadata->relations.relations[i].memberCount;
				for (int k = 0; k < numberOfMembers; k++) {
					//metadata->relations.relations[i].members[k].ref
					for (int s = 0; s < metadata->ways.count; s++) {
						if (metadata->ways.ways[s].id == metadata->relations.relations[i].members[k].ref) {
							//printf("relation ref and way matched %d\n", metadata->ways.ways[s].id);
							rivers.riverList[index].count = numberOfMembers * metadata->ways.ways[s].countNd;
							rivers.riverList[index].riverCoordinates = (Node*)malloc(metadata->ways.ways[s].countNd * sizeof(Node));
							coordinateIndex = 0;
							for (int m = 0; m < metadata->ways.ways[s].countNd; m++) {
								for (int n = 0; n < metadata->nodes.count; n++) {
									if (metadata->ways.ways[s].nd_ids[m] == metadata->nodes.nodes[n].id) {
										//printf("nodeid %lld\n", metadata->ways.ways[s].nd_ids[m]);
										//riversa ata
										rivers.riverList[index].riverCoordinates[coordinateIndex].id = metadata->nodes.nodes[n].id;
										rivers.riverList[index].riverCoordinates[coordinateIndex].lat = metadata->nodes.nodes[n].lat;
										rivers.riverList[index].riverCoordinates[coordinateIndex].lon = metadata->nodes.nodes[n].lon;
										coordinateIndex++;





									}
								}
							}
							index++;
						}
						else {
						}
					}
				}
			}
		}
	}

	printf("NumberofRivers:%d\n", numberOfRivers);
	//loadData
}


void dataManager(Metadata* metadata) {

	if (metadata->nodes.count == 0) {
		printf("No nodes in metadata\n");
	}
	else if (metadata->relations.count == 0) {
		printf("No relation in metadata\n");
	}
	else if (metadata->ways.count == 0) {
		printf("No way in metadata\n");
	}
	else {
		//Part1 
		loadRivers(metadata);
	}
}

