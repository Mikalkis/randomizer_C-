cmake_minimum_required(VERSION 3.16)
project(Rand CXX)

add_executable(rand WIN32 main.cpp app.rc)

target_compile_definitions(rand PRIVATE UNICODE _UNICODE)
target_link_libraries(rand PRIVATE gdi32)

if(MSVC)
    # /utf-8: исходник в UTF-8, содержит русские строки.
    target_compile_options(rand PRIVATE /W4 /EHsc /utf-8)
    # Манифест вшивается из app.rc; автогенерация линкера дала бы второй
    # ресурс MANIFEST с тем же ID 1 (CVT1100 duplicate resource).
    target_link_options(rand PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/MANIFEST:NO>")
else()
    target_compile_options(rand PRIVATE -Wall -Wextra)
endif()
