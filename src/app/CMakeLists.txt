#
# This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.
#

# Get all source files
CollectSourceFiles(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE_SOURCES)

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

# Create the application project
add_executable(DiscordBot ${PRIVATE_SOURCES})

target_link_libraries(DiscordBot
  PRIVATE
    warhead-core-interface
  PUBLIC
    bot)

CollectIncludeDirectories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC_INCLUDES)

target_include_directories(DiscordBot
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(DiscordBot
  PUBLIC
    ${PUBLIC_INCLUDES})

# Install config
CopyApplicationConfig(${CMAKE_CURRENT_SOURCE_DIR} DiscordBot)

install(TARGETS DiscordBot DESTINATION bin)
