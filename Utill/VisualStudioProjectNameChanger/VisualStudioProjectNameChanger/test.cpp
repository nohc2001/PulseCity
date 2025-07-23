#include <Windows.h>
#include <iostream>

bool RenameFolder(const std::wstring& oldPath, const std::wstring& newPath) {
    // MoveFile 함수를 사용하여 폴더 이름 변경
    if (MoveFile(oldPath.c_str(), newPath.c_str())) {
        std::wcout << L"폴더 이름이 성공적으로 변경되었습니다." << std::endl;
        return true;
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"폴더 이름 변경 실패. 오류 코드: " << error << std::endl;
        return false;
    }
}

int main() {
    std::wstring oldFolderName = L"C:\\Users\\nohc2\\OneDrive\\Desktop\\WorkRoom\\Dev\\CollegeProjects\\DumyProject";
    std::wstring newFolderName = L"C:\\Users\\nohc2\\OneDrive\\Desktop\\WorkRoom\\Dev\\CollegeProjects\\Dumy2";

    if (RenameFolder(oldFolderName, newFolderName)) {
        // 성공적으로 폴더 이름이 변경됨
    }

    return 0;
}